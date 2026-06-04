#include "detect_duplicates.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "CLI11.hpp"
#include "doctest.hpp"
#include "r8brain-resampler/CDSPRealFFT.h"
#include "r8brain-resampler/CDSPResampler.h"

#include "audio_data.h"
#include "common.h"
#include "test_helpers.h"

namespace {

// Fingerprinting parameters. Tuned for ~30ms time resolution at 16kHz so we can localise
// sub-second clips inside longer recordings.
constexpr unsigned k_target_sr = 16000;
constexpr int k_fft_bits = 10;       // FFT size = 1024 samples (~64 ms window)
constexpr int k_fft_size = 1 << k_fft_bits;
constexpr int k_hop = 512;           // 50% overlap (~32 ms hop)
constexpr int k_num_bands = 6;       // log-spaced frequency bands for peak picking
constexpr double k_band_min_hz = 200.0;
constexpr double k_band_max_hz = 5000.0;
constexpr int k_fan_out = 5;         // landmarks paired with this many forward peaks
constexpr int k_min_dt = 1;
constexpr int k_max_dt = 63;         // 6 bits
constexpr double k_silence_rms = 1e-4;
// Absolute minimum number of aligned hash hits to consider a match real. Below this, scores
// from very short files are meaningless (a few random collisions can produce high percentages).
constexpr int k_min_absolute_aligned = 25;

struct Landmark {
    uint32_t hash;
    int32_t anchor_frame;
};

struct Fingerprint {
    std::vector<Landmark> landmarks;
    int total_frames = 0;
};

// Mix to mono and resample to k_target_sr using the existing r8brain resampler.
std::vector<double> PreprocessAudio(const AudioData &audio) {
    auto mono = audio.MixDownToMono();
    if (audio.num_channels > 1) {
        for (auto &s : mono) s /= (double)audio.num_channels;
    }
    if (audio.sample_rate == k_target_sr || mono.empty()) return mono;

    const auto out_frames =
        (size_t)((double)mono.size() * (double)k_target_sr / (double)audio.sample_rate);
    std::vector<double> resampled(out_frames);
    r8b::CDSPResampler24 resampler((double)audio.sample_rate, (double)k_target_sr, (int)mono.size());
    resampler.oneshot(mono.data(), (int)mono.size(), resampled.data(), (int)out_frames);
    return resampled;
}

// Boundaries (in FFT bins) of the log-spaced bands used for per-frame peak picking.
std::array<int, k_num_bands + 1> ComputeBandEdges() {
    std::array<int, k_num_bands + 1> edges {};
    const double log_min = std::log(k_band_min_hz);
    const double log_max = std::log(k_band_max_hz);
    for (int i = 0; i <= k_num_bands; ++i) {
        const double f = std::exp(log_min + (log_max - log_min) * (double)i / (double)k_num_bands);
        edges[i] = (int)std::round(f * (double)k_fft_size / (double)k_target_sr);
    }
    return edges;
}

// Compute a Shazam-style landmark fingerprint: log-spaced spectral peaks paired into hashes.
Fingerprint ComputeFingerprint(const std::vector<double> &mono_16k) {
    Fingerprint fp;
    if ((int)mono_16k.size() < k_fft_size) return fp;

    // Hann window (precomputed).
    std::array<double, k_fft_size> window;
    for (int n = 0; n < k_fft_size; ++n) {
        window[n] = 0.5 * (1.0 - std::cos(2.0 * pi * (double)n / (double)(k_fft_size - 1)));
    }

    const auto band_edges = ComputeBandEdges();
    r8b::CDSPRealFFTKeeper fft(k_fft_bits);
    std::vector<double> buf(k_fft_size);
    std::vector<double> mag(k_fft_size / 2 + 1);

    const int num_frames = (int)((mono_16k.size() - k_fft_size) / k_hop) + 1;
    fp.total_frames = num_frames;

    // Per-frame peak list: for each frame, up to k_num_bands peak bins (or -1 if band silent).
    std::vector<std::array<int, k_num_bands>> peaks_per_frame(num_frames);

    for (int frame = 0; frame < num_frames; ++frame) {
        const size_t start = (size_t)frame * (size_t)k_hop;

        // Window and copy.
        double rms = 0;
        for (int n = 0; n < k_fft_size; ++n) {
            const double s = mono_16k[start + (size_t)n];
            rms += s * s;
            buf[n] = s * window[n];
        }
        rms = std::sqrt(rms / (double)k_fft_size);

        if (rms < k_silence_rms) {
            for (int b = 0; b < k_num_bands; ++b) peaks_per_frame[frame][b] = -1;
            continue;
        }

        // Real FFT (Ooura packed format: [DC, Nyquist, Re(1), Im(1), Re(2), Im(2), ...]).
        fft->forward(buf.data());
        mag[0] = std::abs(buf[0]);
        mag[k_fft_size / 2] = std::abs(buf[1]);
        for (int k = 1; k < k_fft_size / 2; ++k) {
            const double re = buf[2 * k];
            const double im = buf[2 * k + 1];
            mag[k] = std::sqrt(re * re + im * im);
        }

        // Pick the strongest bin in each log-spaced band.
        for (int b = 0; b < k_num_bands; ++b) {
            const int lo = band_edges[b];
            const int hi = std::min(band_edges[b + 1], (int)mag.size() - 1);
            int best_bin = -1;
            double best_val = 0;
            double band_sum = 0;
            for (int k = lo; k < hi; ++k) {
                band_sum += mag[k];
                if (mag[k] > best_val) {
                    best_val = mag[k];
                    best_bin = k;
                }
            }
            const double band_mean = band_sum / std::max(1, hi - lo);
            // Require the peak to stick out from the band's mean. Threshold is intentionally
            // loose since silence is already filtered above.
            peaks_per_frame[frame][b] = (best_val > band_mean * 1.5) ? best_bin : -1;
        }
    }

    // Form landmark hashes: each peak (anchor) paired with up to k_fan_out forward peaks.
    fp.landmarks.reserve((size_t)num_frames * k_num_bands * k_fan_out / 4);
    for (int t = 0; t < num_frames; ++t) {
        for (int ab = 0; ab < k_num_bands; ++ab) {
            const int anchor_bin = peaks_per_frame[t][ab];
            if (anchor_bin < 0) continue;
            int paired = 0;
            for (int dt = k_min_dt; dt < k_max_dt && t + dt < num_frames && paired < k_fan_out; ++dt) {
                for (int tb = 0; tb < k_num_bands && paired < k_fan_out; ++tb) {
                    const int target_bin = peaks_per_frame[t + dt][tb];
                    if (target_bin < 0) continue;
                    // 9 bits per freq bin (covers full 0..511) + 6 bits dt = 24-bit hash space,
                    // ~16M buckets — keeps random collisions sparse.
                    const uint32_t f1 = (uint32_t)anchor_bin & 0x1FFu;
                    const uint32_t f2 = (uint32_t)target_bin & 0x1FFu;
                    const uint32_t d = (uint32_t)dt & 0x3Fu;
                    const uint32_t hash = f1 | (f2 << 9) | (d << 18);
                    fp.landmarks.push_back({hash, t});
                    ++paired;
                }
            }
        }
    }
    return fp;
}

struct MatchResult {
    size_t file_a;
    size_t file_b;
    int aligned_hits;
    int delta_frames; // anchor_b - anchor_a
    int span_frames_a; // time-span (in frames) covered by aligned anchors in file A
    double score_a_in_b; // fraction of A's landmarks aligned in B
    double score_b_in_a;
};

// For each pair of files, find the delta-offset histogram peak and report aligned matches.
std::vector<MatchResult> FindMatches(const std::vector<Fingerprint> &fps, double threshold) {
    // Inverted index: hash -> list of (file_index, anchor_frame).
    struct Entry {
        uint32_t file_idx;
        int32_t anchor;
    };
    std::unordered_map<uint32_t, std::vector<Entry>> index;
    index.reserve(1u << 16);
    for (size_t i = 0; i < fps.size(); ++i) {
        for (const auto &lm : fps[i].landmarks) {
            index[lm.hash].push_back({(uint32_t)i, lm.anchor_frame});
        }
    }

    // For each ordered pair (a, b) with a < b, histogram (anchor_b - anchor_a) over all
    // hash collisions. The histogram peak height equals the count of aligned landmarks.
    // We also remember the anchor_a frame for each contributing hit so we can compute the
    // actual time-span covered by the aligned anchors in file A.
    struct PairData {
        // delta -> list of anchor_a frames that matched at this delta
        std::unordered_map<int, std::vector<int>> hits_by_delta;
    };
    std::unordered_map<uint64_t, PairData> pair_data;
    for (auto &kv : index) {
        auto &entries = kv.second;
        if (entries.size() < 2) continue;
        for (size_t i = 0; i < entries.size(); ++i) {
            for (size_t j = i + 1; j < entries.size(); ++j) {
                if (entries[i].file_idx == entries[j].file_idx) continue;
                uint32_t a = entries[i].file_idx, b = entries[j].file_idx;
                int anchor_a = entries[i].anchor;
                int anchor_b = entries[j].anchor;
                if (a > b) {
                    std::swap(a, b);
                    std::swap(anchor_a, anchor_b);
                }
                const int delta = anchor_b - anchor_a;
                const uint64_t key = (uint64_t)a * (uint64_t)fps.size() + (uint64_t)b;
                pair_data[key].hits_by_delta[delta].push_back(anchor_a);
            }
        }
    }

    std::vector<MatchResult> results;
    for (auto &kv : pair_data) {
        const uint64_t key = kv.first;
        const size_t a = (size_t)(key / fps.size());
        const size_t b = (size_t)(key % fps.size());

        int best_delta = 0, best_count = 0;
        const std::vector<int> *best_anchors = nullptr;
        for (auto &dh : kv.second.hits_by_delta) {
            const int count = (int)dh.second.size();
            if (count > best_count) {
                best_count = count;
                best_delta = dh.first;
                best_anchors = &dh.second;
            }
        }
        if (best_count < k_min_absolute_aligned) continue;

        const int la = (int)fps[a].landmarks.size();
        const int lb = (int)fps[b].landmarks.size();
        if (la == 0 || lb == 0) continue;
        const double sa = (double)best_count / (double)la;
        const double sb = (double)best_count / (double)lb;
        if (std::max(sa, sb) < threshold) continue;

        // Span of aligned anchors in file A — i.e. the duration of overlap as it appears in A.
        int min_anchor = INT32_MAX, max_anchor = INT32_MIN;
        for (int t : *best_anchors) {
            if (t < min_anchor) min_anchor = t;
            if (t > max_anchor) max_anchor = t;
        }
        const int span = max_anchor - min_anchor;

        results.push_back({a, b, best_count, best_delta, span, sa, sb});
    }
    std::sort(results.begin(), results.end(), [](const MatchResult &x, const MatchResult &y) {
        return std::max(x.score_a_in_b, x.score_b_in_a) > std::max(y.score_a_in_b, y.score_b_in_a);
    });
    return results;
}

std::string FormatTimestamp(double seconds) {
    const int mins = (int)(seconds / 60.0);
    const double secs = seconds - (double)mins * 60.0;
    return fmt::format("{}:{:05.2f}", mins, secs);
}

double FramesToSeconds(int frames) { return (double)frames * (double)k_hop / (double)k_target_sr; }

} // namespace

CLI::App *DetectDuplicatesCommand::CreateCommandCLI(CLI::App &app) {
    auto cmd = app.add_subcommand(
        "detect-duplicates",
        "Finds audio files that contain similar or identical material, including cases where one "
        "file's audio appears inside another (e.g. a long recording and chops taken from it). "
        "Compares every input file against every other input file using spectral landmark "
        "fingerprints, so it is robust to differences in volume, sample rate and minor encoding "
        "artefacts. Pitch shifts and time-stretches will NOT be detected. Outputs a list of "
        "matches sorted by similarity score.");

    cmd->add_option(
           "--threshold", m_threshold,
           "Minimum similarity score (0.0 to 1.0) for a match to be reported. The score is the "
           "fraction of one file's spectral landmarks that align with the other file at a "
           "consistent time offset. Lower values report more (possibly weaker) matches. "
           "Default is 0.5.")
        ->check(CLI::Range(0.0, 1.0));

    cmd->add_option(
           "--min-overlap-seconds", m_min_overlap_seconds,
           "Ignore matches where the aligned region in either file spans less than this duration. "
           "Helps suppress short incidental matches between unrelated files. Default is 0.5.")
        ->check(CLI::NonNegativeNumber);

    cmd->add_flag("--matrix", m_show_matrix,
                  "Also print a compact pairwise table of all matches that passed the threshold.");

    return cmd;
}

void DetectDuplicatesCommand::ProcessFiles(AudioFiles &files) {
    if (files.Size() < 2) {
        MessageWithNewLine(GetName(), {}, "Need at least 2 input files to compare");
        return;
    }

    // Fingerprint every file.
    std::vector<Fingerprint> fingerprints;
    fingerprints.reserve(files.Size());
    std::vector<const EditTrackedAudioFile *> file_ptrs;
    file_ptrs.reserve(files.Size());

    for (auto &f : files) {
        const auto &audio = f.GetAudio();
        auto mono = PreprocessAudio(audio);
        auto fp = ComputeFingerprint(mono);
        MessageWithNewLine(GetName(), f, "Fingerprinted ({} landmarks)", fp.landmarks.size());
        fingerprints.push_back(std::move(fp));
        file_ptrs.push_back(&f);
    }

    const auto matches = FindMatches(fingerprints, m_threshold);

    // Categorise: containment (one-sided) vs near-duplicate (mutual).
    // Filter by minimum span duration (the time-span of aligned anchors must exceed it).
    std::vector<MatchResult> contained;
    std::vector<MatchResult> duplicates;
    for (const auto &m : matches) {
        const double span_secs = FramesToSeconds(m.span_frames_a);
        if (span_secs < m_min_overlap_seconds) continue;

        const bool a_in_b = m.score_a_in_b >= m_threshold && m.score_b_in_a < m_threshold;
        const bool b_in_a = m.score_b_in_a >= m_threshold && m.score_a_in_b < m_threshold;
        const bool mutual = m.score_a_in_b >= m_threshold && m.score_b_in_a >= m_threshold;
        if (mutual) duplicates.push_back(m);
        else if (a_in_b || b_in_a) contained.push_back(m);
    }

    fmt::print(stderr, "\n=== Near-duplicate pairs ({} found) ===\n", duplicates.size());
    if (duplicates.empty()) {
        fmt::print(stderr, "  (none)\n");
    }
    for (const auto &m : duplicates) {
        const auto na = file_ptrs[m.file_a]->OriginalPath().filename().string();
        const auto nb = file_ptrs[m.file_b]->OriginalPath().filename().string();
        const double span_secs = FramesToSeconds(m.span_frames_a);
        fmt::print(stderr,
                   "  {} == {}  ({:.0f}% / {:.0f}% mutual, ~{:.2f}s aligned, offset {:+.2f}s)\n",
                   na, nb, m.score_a_in_b * 100.0, m.score_b_in_a * 100.0, span_secs,
                   FramesToSeconds(m.delta_frames));
    }

    fmt::print(stderr, "\n=== Containment ({} found) ===\n", contained.size());
    if (contained.empty()) {
        fmt::print(stderr, "  (none)\n");
    }
    for (const auto &m : contained) {
        const auto na = file_ptrs[m.file_a]->OriginalPath().filename().string();
        const auto nb = file_ptrs[m.file_b]->OriginalPath().filename().string();
        const double span_secs = FramesToSeconds(m.span_frames_a);
        if (m.score_a_in_b >= m.score_b_in_a) {
            // A's audio appears inside B at offset = delta in B's timeline
            const double offset_in_b = FramesToSeconds(m.delta_frames);
            fmt::print(stderr,
                       "  {} appears in {} at {} ({:.0f}% of A matched, ~{:.2f}s aligned)\n",
                       na, nb, FormatTimestamp(std::max(0.0, offset_in_b)),
                       m.score_a_in_b * 100.0, span_secs);
        } else {
            const double offset_in_a = FramesToSeconds(-m.delta_frames);
            fmt::print(stderr,
                       "  {} appears in {} at {} ({:.0f}% of B matched, ~{:.2f}s aligned)\n",
                       nb, na, FormatTimestamp(std::max(0.0, offset_in_a)),
                       m.score_b_in_a * 100.0, span_secs);
        }
    }

    if (m_show_matrix) {
        fmt::print(stderr, "\n=== All matches above threshold ===\n");
        for (const auto &m : matches) {
            const auto na = file_ptrs[m.file_a]->OriginalPath().filename().string();
            const auto nb = file_ptrs[m.file_b]->OriginalPath().filename().string();
            fmt::print(stderr, "  {:>40}  <->  {:<40}  A->B {:5.1f}%  B->A {:5.1f}%  hits {}  span {:.2f}s\n",
                       na, nb, m.score_a_in_b * 100.0, m.score_b_in_a * 100.0, m.aligned_hits,
                       FramesToSeconds(m.span_frames_a));
        }
    }
}

TEST_CASE("DetectDuplicatesCommand") {
    SUBCASE("ComputeFingerprint produces landmarks for non-silent audio") {
        auto audio = TestHelpers::CreateSineWaveAtFrequency(1, k_target_sr, 2.0, 1000.0);
        auto mono = PreprocessAudio(audio);
        auto fp = ComputeFingerprint(mono);
        REQUIRE(fp.landmarks.size() > 10);
    }

    SUBCASE("Identical fingerprints have all landmarks aligned at delta 0") {
        auto audio = TestHelpers::CreateSineWaveAtFrequency(1, k_target_sr, 2.0, 1000.0);
        auto mono = PreprocessAudio(audio);
        auto fp_a = ComputeFingerprint(mono);
        auto fp_b = ComputeFingerprint(mono);

        std::vector<Fingerprint> fps {std::move(fp_a), std::move(fp_b)};
        auto matches = FindMatches(fps, 0.5);
        REQUIRE(matches.size() == 1);
        REQUIRE(matches[0].delta_frames == 0);
        REQUIRE(matches[0].score_a_in_b > 0.95);
        REQUIRE(matches[0].score_b_in_a > 0.95);
    }

    SUBCASE("Short clip embedded inside longer file is detected at correct offset") {
        // Build a 3-second source: silence, then a 0.8s 1kHz tone starting at 1.0s, then silence.
        AudioData longer;
        longer.num_channels = 1;
        longer.sample_rate = k_target_sr;
        longer.interleaved_samples.assign((size_t)k_target_sr * 3, 0.0);
        auto tone = TestHelpers::CreateSineWaveAtFrequency(1, k_target_sr, 0.8, 1000.0);
        const size_t insert_frame = k_target_sr; // 1.0 second offset
        for (size_t i = 0; i < tone.interleaved_samples.size(); ++i) {
            longer.interleaved_samples[insert_frame + i] = tone.interleaved_samples[i];
        }

        auto fp_long = ComputeFingerprint(PreprocessAudio(longer));
        auto fp_clip = ComputeFingerprint(PreprocessAudio(tone));

        std::vector<Fingerprint> fps {std::move(fp_clip), std::move(fp_long)};
        auto matches = FindMatches(fps, 0.3);
        REQUIRE(matches.size() == 1);
        // file_a is the clip (smaller index), file_b is the longer file.
        REQUIRE(matches[0].file_a == 0);
        REQUIRE(matches[0].file_b == 1);
        // Almost all of the clip's landmarks should land in the longer file.
        REQUIRE(matches[0].score_a_in_b > 0.8);
        // Very few of the longer file's landmarks should align (it's mostly silence + tone).
        REQUIRE(matches[0].score_b_in_a < matches[0].score_a_in_b);
        // Delta should put the clip at ~1.0s inside the longer file.
        const double offset_secs = FramesToSeconds(matches[0].delta_frames);
        REQUIRE(offset_secs > 0.9);
        REQUIRE(offset_secs < 1.1);
    }

    SUBCASE("Gain difference does not destroy match") {
        auto audio_a = TestHelpers::CreateSineWaveAtFrequency(1, k_target_sr, 2.0, 1000.0);
        auto audio_b = audio_a;
        audio_b.MultiplyByScalar(0.1);
        auto fp_a = ComputeFingerprint(PreprocessAudio(audio_a));
        auto fp_b = ComputeFingerprint(PreprocessAudio(audio_b));

        std::vector<Fingerprint> fps {std::move(fp_a), std::move(fp_b)};
        auto matches = FindMatches(fps, 0.5);
        REQUIRE(matches.size() == 1);
        REQUIRE(matches[0].score_a_in_b > 0.9);
    }
}
