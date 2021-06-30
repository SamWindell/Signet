#include "pitch_drift_corrector.h"

#include <algorithm>

#include "doctest.hpp"
#include "dywapitchtrack/dywapitchtrack.h"

#include "common.h"
#include "defer.h"
#include "gain_calculators.h"
#include "test_helpers.h"
#include "tests_config.h"

class SmoothingFilter {
  public:
    void SetValue(double v, bool hard_reset) {
        m_value = v;
        if (hard_reset) m_prev = m_value;
    }

    double GetSmoothedValue(const double cutoff01 = 0.05) {
        double result = GetSmoothedValueWithoutUpdating(cutoff01);
        m_prev = result;
        return result;
    }

    double GetSmoothedValueWithoutUpdating(const double cutoff01 = 0.05) const {
        return m_prev + cutoff01 * (m_value - m_prev);
    }

  private:
    double m_prev {};
    double m_value {};
};

static bool PitchesAreRoughlyEqual(double a, double b, double cents_deviation_threshold) {
    return std::abs(GetCentsDifference(a, b)) < cents_deviation_threshold;
}

// t is a value from 0 to 1 representing the proportion between f0 and f1 that we want to interpolate a value
// from. A t value of 0 would return f0 and a value of 1 would return f1.
inline double InterpolateCubic(double f0, double f1, double f2, double fm1, double t) {
    return (f0 + (((f2 - fm1 - 3 * f1 + 3 * f0) * t + 3 * (f1 + fm1 - 2 * f0)) * t -
                  (f2 + 2 * fm1 - 6 * f1 + 3 * f0)) *
                     t / 6.0);
}

PitchDriftCorrector::PitchDriftCorrector(const AudioData &data,
                                         std::string_view message_heading,
                                         const fs::path &file_name,
                                         double chunk_length_milliseconds,
                                         bool print_csv)
    : m_message_heading(message_heading)
    , m_file_name(file_name)
    , m_chunk_length_milliseconds(chunk_length_milliseconds)
    , m_sample_rate(data.sample_rate)
    , m_print_csv(print_csv) {

    AudioData mono_data;
    mono_data.num_channels = 1;
    mono_data.sample_rate = data.sample_rate;
    mono_data.bits_per_sample = data.bits_per_sample;
    mono_data.interleaved_samples = data.MixDownToMono();
    const auto &mono_signal = mono_data.interleaved_samples;

    const auto chunk_seconds = m_chunk_length_milliseconds / 1000.0;
    const auto chunk_frames = (usize)(chunk_seconds * data.sample_rate);
    for (usize frame = 0; frame < mono_signal.size(); frame += chunk_frames) {
        const auto chunk_size = (int)std::min(chunk_frames, mono_signal.size() - frame);
        dywapitchtracker pitch_tracker;
        dywapitch_inittracking(&pitch_tracker);
        auto detected_pitch = dywapitch_computepitch(&pitch_tracker, const_cast<double *>(mono_signal.data()),
                                                     (int)frame, chunk_size);
        detected_pitch *= static_cast<double>(data.sample_rate) / 44100.0;
        m_chunks.push_back({
            frame,
            chunk_size,
            detected_pitch,
        });
    }

    if constexpr (k_brute_force_fix_octave_errors) {
        if (const auto whole_file_pitch = mono_data.DetectPitch(); whole_file_pitch) {
            fmt::print("whole file pitch is {}\n", *whole_file_pitch);
            for (auto &chunk : m_chunks) {
                if (!chunk.detected_pitch) continue;
                for (double ratio = 4.0; ratio >= 0.25; ratio /= 2.0) {
                    if (ratio == 1.0) continue;
                    if (PitchesAreRoughlyEqual(chunk.detected_pitch * ratio, *whole_file_pitch, 30)) {
                        fmt::print("fixing octave-error by multiplying pitch {} by {}\n",
                                   chunk.detected_pitch, ratio);
                        chunk.detected_pitch *= ratio;
                        break;
                    }
                }
            }
        }
    }
}

void PitchDriftCorrector::PrintChunkCSV() const {
    if (m_print_csv) {
        fmt::print("detected-pitch,is-outlier,ignore-tuning,target-pitch,pitch-ratio\n");
        for (const auto &c : m_chunks) {
            fmt::print("{:7.2f},{},{},{:7.2f},{:.3f}\n", c.detected_pitch, (int)c.is_detected_pitch_outlier,
                       (int)c.ignore_tuning, c.target_pitch, c.pitch_ratio_for_print);
        }
    }
}

bool PitchDriftCorrector::CanFileBePitchCorrected() const {
    if (m_chunks.size() < 3) {
        MessageWithNewLine(m_message_heading, m_file_name,
                           "The audio is too short to process - it needs to be at least {} milliseconds long",
                           3 * m_chunk_length_milliseconds);
        return false;
    }

    const auto num_detected_pitch_chunks =
        std::count_if(m_chunks.begin(), m_chunks.end(), [](const auto &c) { return c.detected_pitch != 0; });
    constexpr auto minimum_percent_detected = 75.0;
    const auto result =
        (((double)num_detected_pitch_chunks / (double)m_chunks.size()) * 100.0) >= minimum_percent_detected;
    if (!result) {
        WarningWithNewLine(
            m_message_heading, m_file_name,
            "The pitch detection algorithm cannot reliably detect pitch across the duration of the file");
    }
    return result;
}

bool PitchDriftCorrector::ProcessFile(AudioData &data, std::optional<MIDIPitch> expected_midi_pitch) {
    MarkOutlierChunks();
    MarkRegionsToIgnore();
    const auto num_good_regions = MarkTargetPitches();

    if (expected_midi_pitch) {
        for (auto &c : m_chunks) {
            if (!c.ignore_tuning) {
                if (!PitchesAreRoughlyEqual(expected_midi_pitch->pitch, c.target_pitch, 50)) {
                    WarningWithNewLine(
                        m_message_heading, m_file_name,
                        "Failed to process the audio because the detected target pitch ({}) is too far from the expected pitch ({})",
                        c.target_pitch, expected_midi_pitch->pitch);
                    return false;
                }
            }
        }
    }

    defer { PrintChunkCSV(); };
    if (!num_good_regions) {
        WarningWithNewLine(m_message_heading, m_file_name,
                           "Failed to process the audio because there are no regions of consistent pitch");
        return false;
    }

    auto new_interleaved_samples = CalculatePitchCorrectedInterleavedSamples(data);

    const auto size_change_ratio =
        (double)new_interleaved_samples.size() / (double)data.interleaved_samples.size();
    data.interleaved_samples = std::move(new_interleaved_samples);
    data.AudioDataWasStretched(size_change_ratio);

    return true;
}

void PitchDriftCorrector::MarkOutlierChunks() {
    // To detect outliers we work out the most frequent cents-difference between chunks detected pitch (AKA
    // the modal value). However, this is a continuos scale, so we can't just use floating point comparison.
    // Instead, we work out what 'band' each cents-difference is closest too. These bands are set to a
    // reasonable size as specified by cents_band_size. Then we work out the mean cents-difference of all
    // values in the most frequent band. Finally, we use this value to determine if any cents-difference is an
    // outlier, and mark it as such.

    constexpr int cents_band_size = 10;
    constexpr double maximum_cents_difference_between_chunks = 50;

    struct ChunkDiff {
        double cents_diff;
        int nearest_band;
    };

    const auto CreateChunkDiff =
        [&](std::vector<AnalysisChunk>::const_iterator it) -> std::optional<ChunkDiff> {
        const auto curr = it->detected_pitch;
        const auto prev = (it - 1)->detected_pitch;
        if (curr == 0 || prev == 0) return {};
        assert(curr != 0);
        assert(prev != 0);
        ChunkDiff result;
        result.cents_diff = GetCentsDifference(prev, curr);
        result.nearest_band = (int)std::round(result.cents_diff / (double)cents_band_size) * cents_band_size;
        return result;
    };

    std::map<int, int> diff_band_counts;
    for (auto it = m_chunks.begin() + 1; it != m_chunks.end(); ++it) {
        if (const auto diff = CreateChunkDiff(it); diff) {
            ++diff_band_counts[diff->nearest_band];
        }
    }

    const auto mode_band = std::max_element(diff_band_counts.begin(), diff_band_counts.end(),
                                            [](const auto &a, const auto &b) { return a.second < b.second; });
    if (mode_band == diff_band_counts.end()) return;

    double sum = 0.0;
    for (auto it = m_chunks.begin() + 1; it != m_chunks.end(); ++it) {
        if (const auto diff = CreateChunkDiff(it); diff && diff->nearest_band == mode_band->first) {
            sum += std::abs(diff->cents_diff);
        }
    }

    const auto mean_diff_of_mode_band = sum / (double)mode_band->second;
    constexpr double threshold_cents_multiplier = 5; // seems like a reasonable number
    const auto threshold_cents_diff = std::min(mean_diff_of_mode_band * threshold_cents_multiplier,
                                               maximum_cents_difference_between_chunks);
    DebugWithNewLine("outlier detection is checking if adjacent detected pitches are withing {} cents",
                     threshold_cents_diff);

    if (m_chunks.front().detected_pitch == 0) {
        m_chunks.front().is_detected_pitch_outlier = true;
    }

    for (auto it = m_chunks.begin() + 1; it != m_chunks.end(); ++it) {
        const auto prev = (it - 1)->detected_pitch;
        if (!PitchesAreRoughlyEqual(prev, it->detected_pitch, threshold_cents_diff) ||
            it->detected_pitch == 0) {
            it->is_detected_pitch_outlier = true;
            if (it == m_chunks.begin() + 1) {
                m_chunks.begin()->is_detected_pitch_outlier = true;
            }
        }
    }
}

void PitchDriftCorrector::MarkRegionsToIgnore() {
    // Here we analyse the sequence of chunks for regions where there are lots of outliers and mark that whole
    // region as something we should ignore. The detection of 'good' regions and 'bad' regions are determined
    // by the these 2 constexprs.

    // The minimum number of chunks in a row without outliers that should be considered a region for tuning.
    constexpr std::ptrdiff_t min_consecutive_good_chunks = 7;

    // The minimum size of a region to ignore. Long stretches of good audio data may contain a few outliers.
    // Rather than stop and start the tuning for each outlier, we only stop if there is a substantial region
    // of poor pitch data, as specified by this constant. Note this size is not the same as min number of
    // consective outliers.
    constexpr std::ptrdiff_t min_ignore_region_size = 3;

    const auto NextInvalidAnalysisChunk = [&](std::vector<AnalysisChunk>::const_iterator start) {
        return std::find_if(start, m_chunks.cend(),
                            [](const auto &c) { return c.is_detected_pitch_outlier; });
    };

    std::vector<AnalysisChunk>::const_iterator ignore_region_start;
    const auto first_invalid_chunk = NextInvalidAnalysisChunk(m_chunks.begin());
    if (first_invalid_chunk == m_chunks.end()) {
        return;
    }

    if (std::distance(m_chunks.cbegin(), first_invalid_chunk) < min_consecutive_good_chunks) {
        ignore_region_start = m_chunks.begin();
    } else {
        ignore_region_start = first_invalid_chunk;
    }

    std::vector<tcb::span<AnalysisChunk>> ignore_regions;
    auto cursor = ignore_region_start + 1;
    while (true) {
        const auto next_invalid_chunk = NextInvalidAnalysisChunk(cursor);
        const auto distance_to_next_invalid_chunk = std::distance(cursor, next_invalid_chunk);

        if (distance_to_next_invalid_chunk >= min_consecutive_good_chunks ||
            next_invalid_chunk == m_chunks.end()) {
            // handle the end of a ignore-region
            const auto ignore_region_size = std::distance(ignore_region_start, cursor);
            if (ignore_region_size >= min_ignore_region_size) {
                // if the ignore-region is not too small, we register
                ignore_regions.push_back(
                    {(AnalysisChunk *)&*ignore_region_start, (size_t)ignore_region_size});
            }
            ignore_region_start = next_invalid_chunk;
        }

        if (next_invalid_chunk == m_chunks.end()) {
            break;
        }
        cursor = next_invalid_chunk + 1;
    }

    for (auto &r : ignore_regions) {
        for (auto &c : r) {
            c.ignore_tuning = true;
        }
    }
}

double PitchDriftCorrector::FindTargetPitchForChunkRegion(tcb::span<const AnalysisChunk> chunks) {
    // We work out the most-frequent detected pitch by puting them into 'bands' of size cents_band_size, and
    // determining the band with the largest number of entries. The band is based on cents rather than hertz
    // so that we are working on the logarithmic scale which is more useful in this context.
    //
    // Finally, we return the mean pitch of all pitches in the most-frequent band - hopefully recreating what
    // is the most prominent pitch to our ear over time.

    constexpr int cents_band_size = 3;
    std::map<int, int> pitch_band_counts;

    const auto CalcCentsBand = [&](double freq) {
        return (int)(std::round(GetCentsDifference(1, freq) / (double)cents_band_size) * cents_band_size);
    };

    for (const auto &c : chunks) {
        if (c.detected_pitch == 0) continue;
        ++pitch_band_counts[CalcCentsBand(c.detected_pitch)];
    }

    if (!pitch_band_counts.size()) return 0;

    const auto mode_band = std::max_element(pitch_band_counts.begin(), pitch_band_counts.end(),
                                            [](const auto &a, const auto &b) { return a.second < b.second; });

    return std::accumulate(chunks.begin(), chunks.end(), 0.0,
                           [&](double value, const auto &chunk) {
                               const auto cents_band = CalcCentsBand(chunk.detected_pitch);
                               if (cents_band != mode_band->first) return value;
                               return value + chunk.detected_pitch;
                           }) /
           (double)mode_band->second;
}

static double MeanValuesDiff(std::vector<AnalysisChunk>::iterator begin,
                             std::vector<AnalysisChunk>::iterator end,
                             double target_pitch,
                             bool below) {
    size_t count = 0;
    double sum = 0;
    for (auto it = begin; it != end; ++it) {
        if ((!below && it->detected_pitch > target_pitch) || (below && it->detected_pitch < target_pitch)) {
            sum += std::abs(GetCentsDifference(it->detected_pitch, target_pitch));
            count++;
        }
    }
    if (count == 0) return 0;
    return sum / (double)count;
}

int PitchDriftCorrector::MarkTargetPitches() {
    int num_valid_pitch_regions = 0;
    for (auto it = m_chunks.begin(); it != m_chunks.end();) {
        if (it->ignore_tuning) {
            ++it;
            while (it != m_chunks.end() && it->ignore_tuning) {
                ++it;
            }
        } else {
            auto region_start = it;
            while (it != m_chunks.end() && !it->ignore_tuning) {
                ++it;
            }
            auto region_end = it;
            const auto region_size_in_chunks = (size_t)std::distance(region_start, region_end);
            const auto target_pitch = FindTargetPitchForChunkRegion({&*region_start, region_size_in_chunks});
            assert(target_pitch != 0);

            for (auto sub_it = region_start; sub_it != region_end; ++sub_it) {
                sub_it->target_pitch = target_pitch;
            }

            MessageWithNewLine(
                m_message_heading, m_file_name,
                "{}: Found a region for pitch-drift correction from {:.2f} sec to {:.2f} sec; this will be smoothly tuned towards {:.2f} Hz.",
                num_valid_pitch_regions, (double)region_start->frame_start / (double)m_sample_rate,
                (double)((region_end - 1)->frame_start + (region_end - 1)->frame_size) /
                    (double)m_sample_rate,
                target_pitch);
            MessageWithNewLine(m_message_heading, m_file_name,
                               "{}: This region roughly drifts from the target pitch by {:.1f} cents",
                               num_valid_pitch_regions,
                               (MeanValuesDiff(region_start, region_end, target_pitch, false) +
                                MeanValuesDiff(region_start, region_end, target_pitch, true)) /
                                   2.0);
            ++num_valid_pitch_regions;
        }
    }

    if (num_valid_pitch_regions) {
        // if all of the regions have roughly the same pitch, set the target to be the same
        std::vector<double> target_pitches;
        for (auto it = m_chunks.begin(); it != m_chunks.end();) {
            if (!it->ignore_tuning) {
                target_pitches.push_back(it->target_pitch);
                while (it != m_chunks.end() && !it->ignore_tuning) {
                    ++it;
                }
            } else {
                ++it;
            }
        }

        constexpr double cents_threshold = 30;
        bool all_regions_are_same_pitch = true;
        for (const auto &p : target_pitches) {
            for (const auto &p2 : target_pitches) {
                if (&p == &p2) continue;
                if (std::abs(GetCentsDifference(p, p2)) > cents_threshold) {
                    all_regions_are_same_pitch = false;
                    break;
                }
            }
            if (!all_regions_are_same_pitch) break;
        }

        if (all_regions_are_same_pitch) {
            const auto mean_target_pitch =
                std::accumulate(target_pitches.begin(), target_pitches.end(), 0.0) /
                (double)target_pitches.size();

            DebugWithNewLine("Setting whole file to the same target pitch of {}", mean_target_pitch);

            for (auto &chunk : m_chunks) {
                if (!chunk.ignore_tuning) chunk.target_pitch = mean_target_pitch;
            }
        }
    }

    MessageWithNewLine(m_message_heading, m_file_name, "Found {} regions of consistent pitch",
                       num_valid_pitch_regions);
    return num_valid_pitch_regions;
}

std::vector<double> PitchDriftCorrector::CalculatePitchCorrectedInterleavedSamples(const AudioData &data) {
    SmoothingFilter pitch_ratio;

    // Smaller values indicate more smoothing. This value was just determined by hearing what sounded best
    // with a 48000hz file.
    const double smoothing_filter_cutoff = 0.00007 / ((double)data.sample_rate / 48000.0); // very smooth

    auto current_chunk_it = m_chunks.begin();

    double fallback_pitch_ratio = 1.0;
    if (current_chunk_it->ignore_tuning) {
        // Rather than assume that the start of a file is the correct pitch, we instead assume that it is the
        // pitch of the first valid chunk.
        auto first_valid_chunk = current_chunk_it;
        while (first_valid_chunk->ignore_tuning) {
            ++first_valid_chunk;
        }
        if (first_valid_chunk != m_chunks.end()) {
            const auto cents_from_target =
                GetCentsDifference(first_valid_chunk->detected_pitch, first_valid_chunk->target_pitch);
            constexpr double cents_in_octave = 1200;
            const auto new_pitch_ratio = std::exp2(cents_from_target / cents_in_octave);
            fallback_pitch_ratio = new_pitch_ratio;
        }
    }

    const auto UpdatePitchRatio = [&](bool hard_reset) {
        if (current_chunk_it->ignore_tuning) {
            pitch_ratio.SetValue(fallback_pitch_ratio, hard_reset);
        } else {
            fallback_pitch_ratio = 1.0;
            if (!current_chunk_it->is_detected_pitch_outlier) {
                const auto cents_from_target =
                    GetCentsDifference(current_chunk_it->detected_pitch, current_chunk_it->target_pitch);
                constexpr double cents_in_octave = 1200;
                const auto new_pitch_ratio = std::exp2(cents_from_target / cents_in_octave);
                pitch_ratio.SetValue(new_pitch_ratio, hard_reset);
            } else {
                if (hard_reset) {
                    pitch_ratio.SetValue(1, hard_reset);
                }
                // We just don't reset the pitch ratio if this chunk is an outlier in a good region; we are
                // therefore saying that the current pitch ratio should continue as if the outlier did not
                // exist.
            }
        }
        current_chunk_it->pitch_ratio_for_print =
            pitch_ratio.GetSmoothedValueWithoutUpdating(smoothing_filter_cutoff);
    };
    UpdatePitchRatio(true);

    std::vector<double> new_interleaved_samples;
    new_interleaved_samples.reserve(data.interleaved_samples.size());
    double pos = 0;
    while (pos <= (double)data.NumFrames() - 1) {
        const auto pos_index = (s64)pos;
        const auto xm1 = std::max<s64>(pos_index - 1, 0);
        const auto x1 = std::min<s64>(pos_index + 1, data.NumFrames() - 1);
        const auto x2 = std::min<s64>(pos_index + 2, data.NumFrames() - 1);

        const auto t = pos - (double)pos_index;

        for (unsigned chan = 0; chan < data.num_channels; ++chan) {
            const auto new_value = InterpolateCubic(data.GetSample(chan, pos_index), data.GetSample(chan, x1),
                                                    data.GetSample(chan, x2), data.GetSample(chan, xm1), t);
            new_interleaved_samples.push_back(new_value);
        }

        pos += pitch_ratio.GetSmoothedValue(smoothing_filter_cutoff);
        if (pos >= (current_chunk_it->frame_start + current_chunk_it->frame_size)) {
            ++current_chunk_it;
            if (current_chunk_it != m_chunks.end()) {
                UpdatePitchRatio(false);
            }
        }
    }

    return new_interleaved_samples;
}
