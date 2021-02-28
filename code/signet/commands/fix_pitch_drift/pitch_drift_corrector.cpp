#include "pitch_drift_corrector.h"

#include <algorithm>

#include "doctest.hpp"
#include "dywapitchtrack/dywapitchtrack.h"

#include "common.h"
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

PitchDriftCorrector::PitchDriftCorrector(const AudioData &data, std::string_view print_heading)
    : m_print_heading(print_heading) {
    const auto mono_signal = data.MixDownToMono();

    const auto chunk_seconds = k_chunk_length_milliseconds / 1000.0;
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
}

bool PitchDriftCorrector::CanFileBePitchCorrected() const {
    if (m_chunks.size() < 3) {
        MessageWithNewLine(m_print_heading,
                           "The audio is too short to process - it needs to be at least {} milliseconds long",
                           3 * k_chunk_length_milliseconds);
        return false;
    }

    const auto num_detected_pitch_chunks =
        std::count_if(m_chunks.begin(), m_chunks.end(), [](const auto &c) { return c.detected_pitch != 0; });
    constexpr auto minimum_percent_detected = 75.0;
    const auto result =
        (((double)num_detected_pitch_chunks / (double)m_chunks.size()) * 100.0) >= minimum_percent_detected;
    if (!result) {
        MessageWithNewLine(
            m_print_heading,
            "The pitch detection algorithm cannot reliably detect pitch across the duration of the file");
    }
    return result;
}

bool PitchDriftCorrector::ProcessFile(AudioData &data) {
    FixObviousDetectedPitchOutliers();
    MarkOutlierChunks();
    MarkRegionsToIgnore();
    const auto num_good_regions = MarkTargetPitches();
    if (!num_good_regions) {
        MessageWithNewLine(m_print_heading,
                           "Failed to process the audio because there are no regions of consistent pitch");
        return false;
    }

    auto new_interleaved_samples = CalculatePitchCorrectedInterleavedSamples(data);

    for (const auto &c : m_chunks) {
        fmt::print("{:7.2f},{},{},{:7.2f},{}\n", c.detected_pitch, (int)c.is_detected_pitch_outlier,
                   (int)c.ignore_tuning, c.target_pitch, c.pitch_ratio_for_print);
    }

    const auto size_change_ratio =
        (double)new_interleaved_samples.size() / (double)data.interleaved_samples.size();
    data.interleaved_samples = std::move(new_interleaved_samples);
    data.AudioDataWasStretched(size_change_ratio);

    return true;
}

void PitchDriftCorrector::FixObviousDetectedPitchOutliers() {
    // Do a super simple pass where we replace the detected pitch of chunks that were obvious very difference
    // from its previous 2 chunks.

    assert(m_chunks.size() > 2);
    for (usize i = 2; i < m_chunks.size(); ++i) {
        auto &chunk = m_chunks[i];
        const auto &prev = m_chunks[i - 1];
        constexpr double deviation_threshold = 1.006;
        const auto deviation = (chunk.detected_pitch == 0)
                                   ? DBL_MAX
                                   : (std::max(chunk.detected_pitch, prev.detected_pitch) /
                                      std::min(chunk.detected_pitch, prev.detected_pitch));
        if (deviation > deviation_threshold) {
            constexpr double epsilon_cents = 3;
            if (PitchesAreRoughlyEqual(prev.detected_pitch, m_chunks[i - 2].detected_pitch, epsilon_cents)) {
                chunk.detected_pitch = prev.detected_pitch;
            }
        }
    }
}

void PitchDriftCorrector::MarkOutlierChunks() {
    // To detect outliers we work out the most frequent cents-difference between chunks detected pitch (AKA
    // the modal value). However, this is a continuos scale, so we can't just use floating point comparison.
    // Instead, we work out what 'band' each cents-difference is closest too. These bands are set to a
    // reasonable size as specified by cents_band_size. Then we work out the mean cents-difference of all
    // values in the most frequent band. Finally, we use this value to determine if any cents-difference is an
    // outlier, and mark it as such.

    constexpr int cents_band_size = 8;

    struct ChunkDiff {
        ChunkDiff(std::vector<AnalysisChunk>::const_iterator it) {
            const auto curr = it->detected_pitch;
            const auto prev = (it - 1)->detected_pitch;
            cents_diff = GetCentsDifference(prev, curr);
            nearest_band = std::round(cents_diff / (double)cents_band_size) * cents_band_size;
        }
        double cents_diff;
        int nearest_band;
    };

    std::map<int, int> diff_band_counts;
    for (auto it = m_chunks.begin() + 1; it != m_chunks.end(); ++it) {
        ++diff_band_counts[(int)ChunkDiff(it).nearest_band];
    }

    const auto mode_band = std::max_element(diff_band_counts.begin(), diff_band_counts.end(),
                                            [](const auto &a, const auto &b) { return a.second < b.second; });

    double sum = 0.0;
    for (auto it = m_chunks.begin() + 1; it != m_chunks.end(); ++it) {
        ChunkDiff diff(it);
        if (diff.nearest_band != mode_band->first) continue;
        sum += std::abs(diff.cents_diff);
    }

    const auto mean_diff_of_mode_band = sum / (double)mode_band->second;
    constexpr double threshold_cents_multiplier = 3; // seems like a reasonable number
    const auto threshold_cents_diff = mean_diff_of_mode_band * threshold_cents_multiplier;

    for (auto it = m_chunks.begin() + 1; it != m_chunks.end(); ++it) {
        const auto prev = (it - 1)->detected_pitch;
        if (!PitchesAreRoughlyEqual(prev, it->detected_pitch, threshold_cents_diff)) {
            it->is_detected_pitch_outlier = true;
        }
    }
}

void PitchDriftCorrector::MarkRegionsToIgnore() {
    // Here we analyse the sequence of chunks for regions where there are lots of outliers and mark that whole
    // region as something we should ignore. The detection of 'good' regions and 'bad' regions are determined
    // by the these 2 constexprs.

    constexpr std::ptrdiff_t min_consecutive_good_chunks = 7;
    constexpr std::ptrdiff_t min_ignore_region_size = 4;

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
    // Get the min and max detected_pitch of the chunk region, these should be reasonably close toegether
    // due to our previous work in detecting 'good' regions.
    const auto max = std::max_element(chunks.begin(), chunks.end(), [](const auto &a, const auto &b) {
                         return a.detected_pitch < b.detected_pitch;
                     })->detected_pitch;
    const auto min = std::min_element(chunks.begin(), chunks.end(), [](const auto &a, const auto &b) {
                         return a.detected_pitch < b.detected_pitch;
                     })->detected_pitch;

    // Next, we find the mode of the detected pitches. The detected pitches are on a continuous scale. So
    // for this calculation, we break the range (max - min) into an arbitrary number of bands. And then test
    // each pitch to find which band it is closest too.
    constexpr size_t num_value_bands = 5;
    struct ValueBand {
        double value;
        int chunks_within_band;
    };

    std::array<ValueBand, num_value_bands> value_bands;
    for (usize i = 0; i < num_value_bands; ++i) {
        value_bands[i].value = min + (max - min) * (double)i;
        value_bands[i].chunks_within_band = 0;
    }

    struct AnalysisChunkWithBand {
        const AnalysisChunk *chunk;
        const ValueBand *band;
    };

    std::vector<AnalysisChunkWithBand> chunks_with_bands;
    chunks_with_bands.reserve(chunks.size());

    for (const auto &c : chunks) {
        auto closest_band = std::lower_bound(value_bands.begin(), value_bands.end(), c.detected_pitch,
                                             [](const auto &a, const auto &b) { return a.value < b; });
        assert(closest_band != value_bands.end());
        ++closest_band->chunks_within_band;
        chunks_with_bands.push_back({&c, &*closest_band});
    }

    // Find which band is the mode.
    const auto &mode_band_value =
        *std::max_element(value_bands.begin(), value_bands.end(), [](const auto &a, const auto &b) {
            return a.chunks_within_band < b.chunks_within_band;
        });

    // Calculate the mean of all of the detected pitches that are in the mode band.
    return std::accumulate(chunks_with_bands.begin(), chunks_with_bands.end(), 0.0,
                           [&](double value, const AnalysisChunkWithBand &it) {
                               if (it.band != &mode_band_value) return value;
                               return it.chunk->detected_pitch + value;
                           }) /
           (double)mode_band_value.chunks_within_band;
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
            ++num_valid_pitch_regions;
            auto region_start = it;
            while (it != m_chunks.end() && !it->ignore_tuning) {
                ++it;
            }
            auto region_end = it;
            const auto target_pitch = FindTargetPitchForChunkRegion(
                {&*region_start, (size_t)std::distance(region_start, region_end)});
            for (auto sub_it = region_start; sub_it != region_end; ++sub_it) {
                sub_it->target_pitch = target_pitch;
            }
        }
    }
    MessageWithNewLine(m_print_heading, "Found {} regions of consistent pitch", num_valid_pitch_regions);
    return num_valid_pitch_regions;
}

std::vector<double> PitchDriftCorrector::CalculatePitchCorrectedInterleavedSamples(const AudioData &data) {
    SmoothingFilter pitch_ratio;
    constexpr double smoothing_filter_cutoff = 0.00006; // very smooth

    auto current_chunk_it = m_chunks.begin();
    const auto UpdatePitchRatio = [&](bool hard_reset) {
        if (current_chunk_it->ignore_tuning) {
            pitch_ratio.SetValue(1.0, hard_reset);
        } else {
            const auto cents_from_target =
                GetCentsDifference(current_chunk_it->detected_pitch, current_chunk_it->target_pitch);
            constexpr double cents_in_octave = 1200;
            const auto new_pitch_ratio = std::exp2(cents_from_target / cents_in_octave);
            pitch_ratio.SetValue(new_pitch_ratio, hard_reset);
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
