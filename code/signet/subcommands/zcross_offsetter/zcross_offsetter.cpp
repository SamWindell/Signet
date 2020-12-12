#include "zcross_offsetter.h"

#include "doctest.hpp"

#include "test_helpers.h"

size_t ZeroCrossingOffsetter::FindFrameNearestToZeroInBuffer(const tcb::span<const double> interleaved_buffer,
                                                             const size_t num_frames,
                                                             const unsigned num_channels) {
    double minimum_range = DBL_MAX;
    size_t index_of_min = 0;
    for (size_t frame = 0; frame < num_frames; ++frame) {
        double frame_distance = 0;
        for (unsigned channel = 0; channel < num_channels; ++channel) {
            frame_distance += std::abs(interleaved_buffer[frame * num_channels + channel]);
        }
        if (frame_distance < minimum_range) {
            minimum_range = frame_distance;
            index_of_min = frame;
        }
    }
    MessageWithNewLine("ZCross", "Best zero-crossing range is ", minimum_range);
    return index_of_min;
}

bool ZeroCrossingOffsetter::CreateSampleOffsetToNearestZCross(AudioData &audio,
                                                              const AudioDuration &search_size,
                                                              const bool append_skipped_frames_on_end) {
    const auto search_frames = search_size.GetDurationAsFrames(audio.sample_rate, audio.NumFrames());
    MessageWithNewLine("Searching ", search_frames, " frames for a zero-crossing");

    const auto new_start_frame =
        FindFrameNearestToZeroInBuffer(audio.interleaved_samples, search_frames, audio.num_channels);
    if (new_start_frame == 0) {
        MessageWithNewLine("Searching", "No start frame change needed");
        return false;
    }

    MessageWithNewLine("ZCross", "Found best approx zero-crossing frame at position ", new_start_frame);

    auto interleaved_samples_new_start_it =
        audio.interleaved_samples.begin() + new_start_frame * audio.num_channels;
    std::vector<double> new_interleaved_samples {interleaved_samples_new_start_it,
                                                 audio.interleaved_samples.end()};

    if (append_skipped_frames_on_end) {
        new_interleaved_samples.insert(new_interleaved_samples.end(), audio.interleaved_samples.begin(),
                                       interleaved_samples_new_start_it);
        REQUIRE(new_interleaved_samples.size() == audio.interleaved_samples.size());
    }

    audio.interleaved_samples = new_interleaved_samples;
    if (new_start_frame) {
        audio.FramesWereRemovedFromStart(new_start_frame);
    }

    return true;
}

TEST_CASE("[ZCross Offset]") {
    SUBCASE("offsetting a sine wave") {
        const auto buf = TestHelpers::CreateSingleOscillationSineWave(1, 44100, 100);
        REQUIRE(buf.interleaved_samples[0] == std::sin(0));
        REQUIRE(buf.num_channels == 1);

        SUBCASE("finds a zero crossing at the start") {
            REQUIRE(ZeroCrossingOffsetter::FindFrameNearestToZeroInBuffer(buf.interleaved_samples, 10,
                                                                          buf.num_channels) == 0);
        }
        SUBCASE("finds a zero crossing at pi radians") {
            tcb::span<const double> span = buf.interleaved_samples;
            span = span.subspan(buf.NumFrames() / 4);
            REQUIRE(ZeroCrossingOffsetter::FindFrameNearestToZeroInBuffer(span, 60, buf.num_channels) ==
                    buf.NumFrames() / 4);
        }
    }

    SUBCASE("finds smallest in ramp down") {
        std::vector<double> buf(100);
        for (size_t i = 0; i < buf.size(); ++i) {
            buf[i] = (double)(buf.size() - i) / (double)buf.size();
        }

        REQUIRE(ZeroCrossingOffsetter::FindFrameNearestToZeroInBuffer(buf, 1, 1) == 0);
        REQUIRE(ZeroCrossingOffsetter::FindFrameNearestToZeroInBuffer(buf, 10, 1) == 9);
        REQUIRE(ZeroCrossingOffsetter::FindFrameNearestToZeroInBuffer(buf, 20, 1) == 19);
        REQUIRE(ZeroCrossingOffsetter::FindFrameNearestToZeroInBuffer(buf, 100, 1) == 99);
    }

    SUBCASE("empty file") {
        auto buf = TestHelpers::ProcessBufferWithSubcommand<ZeroCrossingOffsetter>("zcross-offset 100ms", {});
    }

    SUBCASE("correctly appends to end") {
        SUBCASE("1 channel") {
            AudioData buf;
            buf.interleaved_samples = {1, 1, 1, 0, -1, -1, -1};
            buf.num_channels = 1;

            auto result = TestHelpers::ProcessBufferWithSubcommand<ZeroCrossingOffsetter>(
                "zcross-offset 4smp --append", buf);
            REQUIRE(result);
            REQUIRE(result->interleaved_samples.size() == buf.interleaved_samples.size());
            REQUIRE(result->interleaved_samples[0] == 0);
            REQUIRE(result->interleaved_samples[4] == 1);
            REQUIRE(result->interleaved_samples[5] == 1);
            REQUIRE(result->interleaved_samples[6] == 1);
        }

        SUBCASE("2 channel") {
            AudioData buf;
            buf.interleaved_samples = {0.5, 0.4, 0.3, 0.2, 0.1, 0.0, 0.9, 0.8, 0.7, 0.6};
            buf.num_channels = 2;

            auto result = TestHelpers::ProcessBufferWithSubcommand<ZeroCrossingOffsetter>(
                "zcross-offset 4smp --append", buf);
            REQUIRE(result);
            REQUIRE(result->interleaved_samples.size() == buf.interleaved_samples.size());

            const std::vector<double> expected {0.1, 0.0, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2};
            for (usize i = 0; i < result->interleaved_samples.size(); ++i) {
                REQUIRE(expected[i] == result->interleaved_samples[i]);
            }
        }
    }

    SUBCASE("search size larger than file will just be clamped") {
        AudioData buf;
        buf.interleaved_samples.resize(10, 1.0);
        buf.num_channels = 1;

        auto result =
            TestHelpers::ProcessBufferWithSubcommand<ZeroCrossingOffsetter>("zcross-offset 100s", buf);
    }
}
