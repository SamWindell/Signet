#include "mir_analysis.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "r8brain-resampler/CDSPRealFFT.h"

#include "audio_data.h"
#include "common.h"
#include "midi_pitches.h"

namespace mir {

namespace {

constexpr int k_fft_bits = 12;
constexpr int k_fft_size = 1 << k_fft_bits;
constexpr int k_hop = k_fft_size / 2;

double PearsonCorrelation(const AudioData &audio) {
    const auto frames = audio.NumFrames();
    if (audio.num_channels < 2 || frames == 0) return 0.0;
    double sum_ll = 0, sum_rr = 0, sum_lr = 0;
    for (size_t i = 0; i < frames; ++i) {
        const double l = audio.GetSample(0, i);
        const double r = audio.GetSample(1, i);
        sum_ll += l * l;
        sum_rr += r * r;
        sum_lr += l * r;
    }
    const double denom = std::sqrt(sum_ll * sum_rr);
    if (denom < 1e-20) return 0.0;
    return sum_lr / denom;
}

std::vector<double> MonoMix(const AudioData &audio) {
    auto mono = audio.MixDownToMono();
    if (audio.num_channels > 1) {
        for (auto &s : mono) s /= (double)audio.num_channels;
    }
    return mono;
}

// Averaged per-bin magnitude across all overlapping Hann-windowed frames,
// already scaled so a unit-amplitude sine reads ~0 dBFS at its bin.
std::vector<double> AveragedMagnitudes(const std::vector<double> &mono) {
    std::vector<double> mag(k_fft_size / 2 + 1, 0.0);
    if ((int)mono.size() < k_fft_size) return mag;

    std::vector<double> window(k_fft_size);
    for (int n = 0; n < k_fft_size; ++n) {
        window[n] = 0.5 * (1.0 - std::cos(2.0 * pi * (double)n / (double)(k_fft_size - 1)));
    }
    double window_sum = 0;
    for (double w : window) window_sum += w;
    const double window_norm = window_sum / (double)k_fft_size;

    r8b::CDSPRealFFTKeeper fft(k_fft_bits);
    std::vector<double> buf(k_fft_size);
    const int num_frames = (int)((mono.size() - k_fft_size) / k_hop) + 1;
    for (int frame = 0; frame < num_frames; ++frame) {
        const size_t start = (size_t)frame * (size_t)k_hop;
        for (int n = 0; n < k_fft_size; ++n) buf[n] = mono[start + (size_t)n] * window[n];
        fft->forward(buf.data());
        mag[0] += std::abs(buf[0]);
        mag[k_fft_size / 2] += std::abs(buf[1]);
        for (int k = 1; k < k_fft_size / 2; ++k) {
            const double re = buf[2 * k];
            const double im = buf[2 * k + 1];
            mag[k] += std::sqrt(re * re + im * im);
        }
    }
    const double scale = 2.0 / ((double)k_fft_size * (double)num_frames * window_norm);
    for (auto &m : mag) m *= scale;
    return mag;
}

nlohmann::json BinMagnitudesToBands(const std::vector<double> &mag, double sr, int num_bands) {
    nlohmann::json out = nlohmann::json::object();
    auto db_arr = nlohmann::json::array();
    const double f_min = 20.0;
    const double f_max = sr * 0.5;
    out["unit"] = "dbfs";
    out["scale"] = "log";
    out["min"] = f_min;
    out["max"] = f_max;
    if (mag.empty() || num_bands <= 0) {
        out["values"] = db_arr;
        return out;
    }
    const double hz_per_bin = sr / (double)k_fft_size;
    const double log_min = std::log(f_min);
    const double log_max = std::log(f_max);
    for (int b = 0; b < num_bands; ++b) {
        const double lo_hz = std::exp(log_min + (log_max - log_min) * (double)b / (double)num_bands);
        const double hi_hz =
            std::exp(log_min + (log_max - log_min) * (double)(b + 1) / (double)num_bands);
        const int lo_bin = std::max(1, (int)std::floor(lo_hz / hz_per_bin));
        const int hi_bin = std::min((int)mag.size(), (int)std::ceil(hi_hz / hz_per_bin) + 1);
        double power_sum = 0;
        int n = 0;
        for (int k = lo_bin; k < hi_bin; ++k) {
            power_sum += mag[k] * mag[k];
            ++n;
        }
        if (n == 0) {
            db_arr.push_back(-120.0);
            continue;
        }
        const double rms_mag = std::sqrt(power_sum / (double)n);
        const double db = (rms_mag > 1e-12) ? 20.0 * std::log10(rms_mag) : -120.0;
        db_arr.push_back(db);
    }
    out["values"] = db_arr;
    return out;
}

// Aggregates the per-chunk pitch track into a fixed number of columns aligned to the envelope.
// Each entry is the median voiced pitch in Hz for that time slice, or null if no chunks in
// the slice were pitched.
nlohmann::json ComputePitchTrack(const AudioData &audio, int columns) {
    nlohmann::json out = nlohmann::json::object();
    auto hz_arr = nlohmann::json::array();
    const double total_seconds =
        (audio.sample_rate > 0) ? (double)audio.NumFrames() / (double)audio.sample_rate : 0.0;
    out["unit"] = "hz";
    out["scale"] = "linear";
    out["min"] = 0.0;
    out["max"] = total_seconds;
    out["null_means"] = "unvoiced";
    out["values"] = hz_arr;

    if (columns <= 0 || audio.NumFrames() == 0 || audio.sample_rate == 0) return out;

    const auto track = audio.DetectPitchTrack();
    if (track.empty()) return out;
    if (total_seconds <= 0.0) return out;
    const double col_seconds = total_seconds / (double)columns;

    size_t idx = 0;
    for (int c = 0; c < columns; ++c) {
        const double t_hi = (c == columns - 1) ? total_seconds + 1.0 : (double)(c + 1) * col_seconds;
        std::vector<double> voiced_hz;
        int total = 0;
        while (idx < track.size() && track[idx].time_seconds < t_hi) {
            ++total;
            if (track[idx].hz > 0.0) voiced_hz.push_back(track[idx].hz);
            ++idx;
        }
        if (total == 0) {
            // No new chunks land in this column (track is coarser than the column grid).
            // Carry the nearest chunk so the timeline stays gapless.
            const size_t nearest = (idx == 0) ? 0 : idx - 1;
            const auto &e = track[nearest];
            hz_arr.push_back(e.hz > 0.0 ? nlohmann::json(e.hz) : nlohmann::json(nullptr));
            continue;
        }
        if (voiced_hz.empty()) {
            hz_arr.push_back(nullptr);
        } else {
            std::sort(voiced_hz.begin(), voiced_hz.end());
            hz_arr.push_back(voiced_hz[voiced_hz.size() / 2]);
        }
    }
    out["values"] = std::move(hz_arr);
    return out;
}

nlohmann::json ComputeEnvelope(const AudioData &audio, int columns) {
    nlohmann::json out = nlohmann::json::object();
    auto values = nlohmann::json::array();
    out["unit"] = "rms_linear";
    out["scale"] = "linear";
    out["min"] = 0.0;
    out["max"] =
        (audio.sample_rate > 0) ? (double)audio.NumFrames() / (double)audio.sample_rate : 0.0;
    const auto frames = audio.NumFrames();
    if (frames == 0 || columns <= 0) {
        out["values"] = values;
        return out;
    }
    const size_t per_col = std::max<size_t>(1, frames / (size_t)columns);
    for (int c = 0; c < columns; ++c) {
        const size_t lo = (size_t)c * per_col;
        const size_t hi = std::min(frames, lo + per_col);
        if (lo >= hi) break;
        double sumsq = 0;
        size_t n = 0;
        for (size_t i = lo; i < hi; ++i) {
            double s = 0;
            for (unsigned ch = 0; ch < audio.num_channels; ++ch) s += audio.GetSample(ch, i);
            s /= (double)audio.num_channels;
            sumsq += s * s;
            ++n;
        }
        values.push_back(std::sqrt(sumsq / (double)n));
    }
    out["values"] = std::move(values);
    return out;
}

} // namespace

nlohmann::json Analyse(const AudioData &audio, int envelope_columns, int spectrum_bands) {
    nlohmann::json j;
    j["length_seconds"] = (double)audio.NumFrames() / (double)audio.sample_rate;
    j["channels"] = audio.num_channels;

    if (auto const pitch = audio.DetectPitchWithConfidence()) {
        const auto note = FindClosestMidiPitch(pitch->hz);
        j["pitch"] = {
            {"hz", pitch->hz},
            {"confidence", pitch->confidence},
            {"nearest_note", note.name},
            {"midi_note", note.midi_note},
            {"cents", GetCentsDifference(note.pitch, pitch->hz)},
        };
    } else {
        j["pitch"] = nullptr;
    }

    const bool stereo = audio.num_channels >= 2;
    j["stereo"] = stereo;
    if (stereo) j["phase_correlation"] = PearsonCorrelation(audio);
    else j["phase_correlation"] = nullptr;

    const auto mono = MonoMix(audio);
    const auto mag = AveragedMagnitudes(mono);
    j["spectrum"] = BinMagnitudesToBands(mag, (double)audio.sample_rate, spectrum_bands);
    j["envelope"] = ComputeEnvelope(audio, envelope_columns);
    j["pitch_track"] = ComputePitchTrack(audio, envelope_columns);
    return j;
}

} // namespace mir
