#include "realtime_auto_tune.h"

#include "CLI11.hpp"
#include "doctest.hpp"
#include "dywapitchtrack/dywapitchtrack.h"

#include "audio_files.h"
#include "common.h"
#include "gain_calculators.h"
#include "midi_pitches.h"

CLI::App *RealTimeAutoTuneCommand::CreateCommandCLI(CLI::App &app) {
    auto auto_tune = app.add_subcommand("real-time-auto-tune", "");
    return auto_tune;
}

struct Chunk {
    usize frame_start;
    int frame_size;
    double detected_pitch;
    double rms;

    bool is_outlier;
};

template <>
struct fmt::formatter<Chunk> {
    constexpr auto parse(format_parse_context &ctx) { return ctx.end(); }

    template <typename FormatContext>
    auto format(const Chunk &c, FormatContext &ctx) {
        return format_to(ctx.out(), "{},{},{}", c.detected_pitch, c.rms, c.is_outlier);
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
            const auto &prev_prev = chunks[i - 2];
            if (PitchesAreRoughlyEqual(prev.detected_pitch, prev_prev.detected_pitch, 2)) {
                chunk.detected_pitch = prev.detected_pitch;
            }
        }
    }
}

void MarkOutliers(std::vector<Chunk> &chunks) {
    assert(chunks.size() > 2);

    RingBuffer moving_average;
    moving_average.Add(chunks[0].detected_pitch);

    fmt::print("cents from 392.5 and 390: {}\n", GetCentsDifference(392.5, 390));

    for (usize i = 0; i < chunks.size(); ++i) {
        auto &curr = chunks[i];
        const auto prev = chunks[(i == 0) ? i : (i - 1)];

        const auto mean_pitch = moving_average.Mean();
        int is_outlier = 0;
        const auto cents_from_mean = std::abs(GetCentsDifference(curr.detected_pitch, mean_pitch));
        if (!PitchesAreRoughlyEqual(curr.detected_pitch, mean_pitch, 3)) {
            is_outlier = 1;
        }

        moving_average.Add(curr.detected_pitch);

        curr.is_outlier = is_outlier != 0;
    }
}

bool ApproxEqual(double lhs, double rhs, double epsilon) {
    return std::fabs(lhs - rhs) < epsilon * (1 + std::max(std::fabs(lhs), std::fabs(rhs)));
}

bool DoesFileHaveReliablePitchData(std::vector<Chunk> &chunks) {
    const auto num_detected_pitch_chunks =
        std::count_if(chunks.begin(), chunks.end(), [](const auto &c) { return c.detected_pitch != 0; });
    constexpr auto minimum_percent_detected = 90.0;
    const auto result =
        (((double)num_detected_pitch_chunks / (double)chunks.size()) * 100.0) >= minimum_percent_detected;
    if (!result) {
        MessageWithNewLine(
            "RTAutoTune",
            "The pitch detection algorithm cannot reliably detect pitch across the duration of the file");
    }
    return result;
}

void RealTimeAutoTuneCommand::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        const auto &audio = f.GetAudio();

        ForEachDeinterleavedChannel(
            f.GetWritableAudio().interleaved_samples, audio.num_channels,
            [&](const std::vector<double> &data, unsigned channel) {
                fmt::print("channel {}\n", channel);
                bool silent = true;
                for (const auto &s : data) {
                    if (s != 0) {
                        silent = false;
                        break;
                    }
                }
                if (silent) {
                    fmt::print("channel is silent\n");
                    return;
                }

                (void)channel;
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
                    chunks.push_back(
                        {frame, chunk_size, detected_pitch, GetRMS({&data[frame], (usize)chunk_size})});
                }

                if (!DoesFileHaveReliablePitchData(chunks)) {
                    fmt::print("channel does not have reliable pitch data\n");
                    return;
                }
                FixObviousOutliers(chunks);
                MarkOutliers(chunks);
                // MarkPitchToUse(chunks);

                for (const auto &c : chunks) {
                    fmt::print("{}\n", c);
                }
            });
    }
}

TEST_CASE("RealTimeAutoTune") {
    SUBCASE("ApproxEqual") {
        CHECK(ApproxEqual(0.1, 0.15, 0.05));
        CHECK(ApproxEqual(0.1, 0.14, 0.05));
        CHECK(!ApproxEqual(0.1, 0.16, 0.05));
        CHECK(ApproxEqual(0.11, 0.1, 0.05));
        CHECK(!ApproxEqual(0.16, 0.1, 0.05));
    }
}
