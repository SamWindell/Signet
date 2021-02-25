#include "realtime_auto_tune.h"

#include <optional>

#include "CLI11.hpp"
#include "doctest.hpp"
#include "dywapitchtrack/dywapitchtrack.h"

#include "audio_files.h"
#include "common.h"
#include "gain_calculators.h"
#include "midi_pitches.h"
#include "test_helpers.h"
#include "tests_config.h"

CLI::App *RealTimeAutoTuneCommand::CreateCommandCLI(CLI::App &app) {
    auto auto_tune = app.add_subcommand("rt-auto-tune", "");
    return auto_tune;
}

// A chunk represents
struct Chunk {
    usize frame_start {};
    int frame_size {};
    double detected_pitch {};

    bool is_invalid {};
    bool ignore_tuning {};
    double target_pitch {};

    double pitch_ratio_for_print {};
};

template <>
struct fmt::formatter<Chunk> {
    constexpr auto parse(format_parse_context &ctx) { return ctx.end(); }

    template <typename FormatContext>
    auto format(const Chunk &c, FormatContext &ctx) {
        return format_to(ctx.out(), "{},{},{},{},{}", c.detected_pitch, c.is_invalid, c.ignore_tuning,
                         c.target_pitch, c.pitch_ratio_for_print);
    }
};

class RingBuffer {
  public:
    void Fill(double value) { std::fill(buffer.begin(), buffer.end(), value); }
    void Add(double value) {
        num_added++;
        buffer[current_index] = value;
        ++current_index;
        if (current_index == buffer.size()) {
            current_index = 0;
        }
    }
    double Mean() const {
        assert(num_added);
        if (num_added < buffer.size()) {
            return std::accumulate(buffer.begin(), buffer.begin() + num_added, 0.0) / (double)num_added;
        } else {
            return std::accumulate(buffer.begin(), buffer.end(), 0.0) / (double)buffer.size();
        }
    }

  private:
    usize num_added = 0;
    usize current_index {};
    std::array<double, 5> buffer {};
};

bool PitchesAreRoughlyEqual(double a, double b, double cents_deviation_threshold) {
    return std::abs(GetCentsDifference(a, b)) < cents_deviation_threshold;
}

void FixObviousOutliers(std::vector<Chunk> &chunks) {
    assert(chunks.size() > 2);
    for (usize i = 2; i < chunks.size(); ++i) {
        auto &chunk = chunks[i];
        const auto &prev = chunks[i - 1];
        constexpr double deviation_threshold = 1.006;
        const auto deviation = (chunk.detected_pitch == 0)
                                   ? DBL_MAX
                                   : (std::max(chunk.detected_pitch, prev.detected_pitch) /
                                      std::min(chunk.detected_pitch, prev.detected_pitch));
        if (deviation > deviation_threshold) {
            constexpr double epsilon_cents = 3;
            if (PitchesAreRoughlyEqual(prev.detected_pitch, chunks[i - 2].detected_pitch, epsilon_cents)) {
                chunk.detected_pitch = prev.detected_pitch;
            }
        }
    }
}

void MarkInvalidChunks(std::vector<Chunk> &chunks) {
    RingBuffer moving_average;
    moving_average.Add(chunks[0].detected_pitch);

    for (auto &chunk : chunks) {
        // If the chunk's detected pitch is too different from a moving average, mark it as potentially
        // erroneous
        const auto mean_pitch = moving_average.Mean();
        constexpr double max_cents_deviation_from_mean = 3;
        if (!PitchesAreRoughlyEqual(chunk.detected_pitch, mean_pitch, max_cents_deviation_from_mean)) {
            chunk.is_invalid = true;
        }

        moving_average.Add(chunk.detected_pitch);
    }
}

bool DoesFileHaveReliablePitchData(std::vector<Chunk> &chunks) {
    const auto num_detected_pitch_chunks =
        std::count_if(chunks.begin(), chunks.end(), [](const auto &c) { return c.detected_pitch != 0; });
    constexpr auto minimum_percent_detected = 75.0;
    const auto result =
        (((double)num_detected_pitch_chunks / (double)chunks.size()) * 100.0) >= minimum_percent_detected;
    if (!result) {
        MessageWithNewLine(
            "RTAutoTune",
            "The pitch detection algorithm cannot reliably detect pitch across the duration of the file");
    }
    return result;
}

void MarkRegionsToIgnore(const std::vector<Chunk> &chunks) {
    const auto NextInvalidChunk = [&](std::vector<Chunk>::const_iterator start) {
        return std::find_if(start, chunks.end(), [](const auto &c) { return c.is_invalid; });
    };

    constexpr std::ptrdiff_t min_consecutive_good_chunks = 7;
    constexpr std::ptrdiff_t min_ignore_region_size = 4;

    std::vector<Chunk>::const_iterator ignore_region_start;
    const auto first_invalid_chunk = NextInvalidChunk(chunks.begin());
    if (first_invalid_chunk == chunks.end()) {
        return;
    }

    if (std::distance(chunks.begin(), first_invalid_chunk) < min_consecutive_good_chunks) {
        ignore_region_start = chunks.begin();
    } else {
        ignore_region_start = first_invalid_chunk;
    }

    std::vector<tcb::span<Chunk>> ignore_regions;
    std::vector<Chunk>::const_iterator cursor = ignore_region_start + 1;
    while (true) {
        const auto next_invalid_chunk = NextInvalidChunk(cursor);
        const auto distance_to_next_invalid_chunk = std::distance(cursor, next_invalid_chunk);

        if (distance_to_next_invalid_chunk >= min_consecutive_good_chunks ||
            next_invalid_chunk == chunks.end()) {
            // handle the end of a ignore-region
            const auto ignore_region_size = std::distance(ignore_region_start, cursor);
            if (ignore_region_size >= min_ignore_region_size) {
                // if the ignore-region is not too small, we register
                ignore_regions.push_back({(Chunk *)&*ignore_region_start, (size_t)ignore_region_size});
            }
            ignore_region_start = next_invalid_chunk;
        }

        if (next_invalid_chunk == chunks.end()) {
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

double TargetPitchForChunkRegion(tcb::span<const Chunk> chunks) {
    // Get the min and max detected_pitch of the chunk region, these should be reasonably close toegether due
    // to our previous work in detecting 'good' regions.
    const auto max = std::max_element(chunks.begin(), chunks.end(), [](const auto &a, const auto &b) {
                         return a.detected_pitch < b.detected_pitch;
                     })->detected_pitch;
    const auto min = std::min_element(chunks.begin(), chunks.end(), [](const auto &a, const auto &b) {
                         return a.detected_pitch < b.detected_pitch;
                     })->detected_pitch;

    // Next, we find the median of the detected pitches. The detected pitches are on a continuous scale. So
    // for this calculation, we break range (max - min) into an arbitrary number of bands. And then test each
    // pitch to find which band it is closest too.
    constexpr size_t num_value_bands = 5;
    struct ValueBand {
        double value;
        int count;
    };

    std::array<ValueBand, num_value_bands> value_bands;
    for (usize i = 0; i < num_value_bands; ++i) {
        value_bands[i].value = min + (max - min) * (double)i;
        value_bands[i].count = 0;
    }

    struct ChunkWithBand {
        const Chunk *chunk;
        const ValueBand *band;
    };

    std::vector<ChunkWithBand> chunks_with_bands;

    for (const auto &c : chunks) {
        auto closest_band = std::lower_bound(value_bands.begin(), value_bands.end(), c.detected_pitch,
                                             [](const auto &a, const auto &b) { return a.value < b; });
        assert(closest_band != value_bands.end());
        ++closest_band->count;
        chunks_with_bands.push_back({&c, &*closest_band});
    }

    // Find which band is the median.
    const auto &median_band_value =
        *std::max_element(value_bands.begin(), value_bands.end(),
                          [](const auto &a, const auto &b) { return a.count < b.count; });

    // Calculate the mean of all of the detected pitches that are in the median band.
    return std::accumulate(chunks_with_bands.begin(), chunks_with_bands.end(), 0.0,
                           [&](double value, const ChunkWithBand &it) {
                               if (it.band != &median_band_value) return value;
                               return it.chunk->detected_pitch + value;
                           }) /
           (double)median_band_value.count;
}

void MarkTargetPitches(std::vector<Chunk> &chunks) {
    for (auto it = chunks.begin(); it != chunks.end();) {
        if (it->ignore_tuning) {
            ++it;
            while (it != chunks.end() && it->ignore_tuning) {
                ++it;
            }
        } else {
            auto region_start = it;
            while (it != chunks.end() && !it->ignore_tuning) {
                ++it;
            }
            auto region_end = it;
            const auto target_pitch =
                TargetPitchForChunkRegion({&*region_start, (size_t)std::distance(region_start, region_end)});
            for (auto sub_it = region_start; sub_it != region_end; ++sub_it) {
                sub_it->target_pitch = target_pitch;
            }
        }
    }
}

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

// t is a value from 0 to 1 representing the proportion between f0 and f1 that we want to interpolate a value
// from. A t value of 0 would return f0 and a value of 1 would return f1.
inline double InterpolateCubic(double f0, double f1, double f2, double fm1, double t) {
    return (f0 + (((f2 - fm1 - 3 * f1 + 3 * f0) * t + 3 * (f1 + fm1 - 2 * f0)) * t -
                  (f2 + 2 * fm1 - 6 * f1 + 3 * f0)) *
                     t / 6.0);
}

std::vector<double> Retune(std::vector<Chunk> &chunks, const std::vector<double> &input_data) {
    SmoothingFilter pitch_ratio;
    constexpr double smoothing_filter_cutoff = 0.00007; // very smooth

    auto current_chunk_it = chunks.begin();
    const auto UpdatePitchRatio = [&](bool hard_reset) {
        if (current_chunk_it->ignore_tuning) {
            pitch_ratio.SetValue(1.0, hard_reset);
        } else {
            const auto cents_from_target =
                GetCentsDifference(current_chunk_it->detected_pitch, current_chunk_it->target_pitch);
            const auto new_pitch_ratio = std::exp2((cents_from_target / 100.0) / 12.0);
            pitch_ratio.SetValue(new_pitch_ratio, hard_reset);
        }
        current_chunk_it->pitch_ratio_for_print =
            pitch_ratio.GetSmoothedValueWithoutUpdating(smoothing_filter_cutoff);
    };
    UpdatePitchRatio(true);

    std::vector<double> result;
    result.reserve(input_data.size());
    double pos = 0;
    while (pos <= (double)input_data.size() - 1) {
        const auto pos_index = (s64)pos;
        const auto xm1 = std::max<s64>(pos_index - 1, 0);
        const auto x1 = std::min<s64>(pos_index + 1, input_data.size() - 1);
        const auto x2 = std::min<s64>(pos_index + 2, input_data.size() - 1);

        const auto t = pos - (double)pos_index;
        const auto new_value =
            InterpolateCubic(input_data[pos_index], input_data[x1], input_data[x2], input_data[xm1], t);
        result.push_back(new_value);

        pos += pitch_ratio.GetSmoothedValue(smoothing_filter_cutoff);
        if (pos >= (current_chunk_it->frame_start + current_chunk_it->frame_size)) {
            ++current_chunk_it;
            if (current_chunk_it != chunks.end()) {
                UpdatePitchRatio(false);
            }
        }
    }

    return result;
}

void RealTimeAutoTuneCommand::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        const auto &audio = f.GetAudio();
        std::vector<std::vector<double>> channels(audio.num_channels);
        bool cant_be_auto_tuned = false;

        ForEachDeinterleavedChannel(
            f.GetWritableAudio().interleaved_samples, audio.num_channels,
            [&](const std::vector<double> &data, unsigned channel) {
                if (cant_be_auto_tuned) return;
                bool silent = true;
                for (const auto &s : data) {
                    if (s != 0) {
                        silent = false;
                        break;
                    }
                }
                if (silent) {
                    channels[channel] = data;
                    return;
                }

                std::vector<Chunk> chunks;
                constexpr auto chunk_seconds = 0.05;
                const auto chunk_frames = (usize)(chunk_seconds * audio.sample_rate);
                for (usize frame = 0; frame < data.size(); frame += chunk_frames) {
                    const auto chunk_size = (int)std::min(chunk_frames, data.size() - frame);
                    dywapitchtracker pitch_tracker;
                    dywapitch_inittracking(&pitch_tracker);
                    auto detected_pitch = dywapitch_computepitch(
                        &pitch_tracker, const_cast<double *>(data.data()), (int)frame, chunk_size);
                    if (audio.sample_rate != 44100) {
                        detected_pitch *= static_cast<double>(audio.sample_rate) / 44100.0;
                    }
                    chunks.push_back({
                        frame,
                        chunk_size,
                        detected_pitch,
                    });
                }

                if (!DoesFileHaveReliablePitchData(chunks)) {
                    cant_be_auto_tuned = true;
                    return;
                }
                FixObviousOutliers(chunks);
                MarkInvalidChunks(chunks);
                MarkRegionsToIgnore(chunks);
                MarkTargetPitches(chunks);
                auto result = Retune(chunks, data);
                channels[channel] = result;

                fmt::print(
                    "detected_pitch,is_invalid_chunk,is_part_of_ignored_region,target_pitch_to_tune_to,pitch_ratio_at_this_point\n");
                for (const auto &c : chunks) {
                    fmt::print("{}\n", c);
                }
            });

        if (cant_be_auto_tuned) {
            WarningWithNewLine(
                GetName(),
                "File can't be real-time auto-tuned because one or more channels to not have reliable enough pitch data");
            continue;
        }

        // TODO: This is a hack. The samples should be retuned together to ensure alignment.
        const auto max_samples =
            std::max_element(channels.begin(), channels.end(), [](const auto &a, const auto &b) {
                return a.size() < b.size();
            })->size();
        for (auto &c : channels) {
            c.resize(max_samples);
        }

        std::vector<double> interleaved_samples;
        interleaved_samples.reserve(max_samples * channels.size());
        for (usize i = 0; i < max_samples; ++i) {
            for (auto &c : channels) {
                interleaved_samples.push_back(c[i]);
            }
        }

        const auto prev_num_frames = audio.interleaved_samples.size() / channels.size();

        f.GetWritableAudio().interleaved_samples = interleaved_samples;
        const auto stretch_ratio = (double)f.GetWritableAudio().NumFrames() / (double)prev_num_frames;
        f.GetWritableAudio().AudioDataWasStretched(stretch_ratio);

        MessageWithNewLine(GetName(), "File successfully auto-tuned.");
    }
}

AudioData CreateSineWaveDriftingPitch() {
    const unsigned sample_rate = 44410;
    const unsigned num_frames = sample_rate * 2;
    const double frequency_hz = 440;

    const auto oscillations_per_sec = frequency_hz;
    const auto oscillations_in_whole = oscillations_per_sec * 2;
    const auto taus_in_whole = oscillations_in_whole * 2 * pi;
    const auto taus_per_sample = taus_in_whole / num_frames;

    AudioData buf;
    buf.num_channels = 1;
    buf.sample_rate = sample_rate;
    buf.interleaved_samples.resize(num_frames * 1);
    double phase = -pi * 2;
    for (size_t frame = 0; frame < num_frames; ++frame) {
        const double value = (double)std::sin(phase);
        phase += taus_per_sample;
        phase *= 1.0000002;
        buf.interleaved_samples[frame] = value;
    }
    return buf;
}

TEST_CASE("RealTimeAutoTune") {
    const auto buf = CreateSineWaveDriftingPitch();
    WriteAudioFile(fs::path(BUILD_DIRECTORY) / "rtat-drifting-sine.wav", buf, {});

    const auto out = TestHelpers::ProcessBufferWithCommand<RealTimeAutoTuneCommand>("rt-auto-tune", buf);
    REQUIRE(out);
    WriteAudioFile(fs::path(BUILD_DIRECTORY) / "rtat-drifting-sine-processed.wav", *out, {});
}
