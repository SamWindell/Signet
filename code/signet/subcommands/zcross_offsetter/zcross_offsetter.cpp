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
    std::cout << "Best zero-crossing range is " << minimum_range << "\n";
    return index_of_min;
}

AudioFile ZeroCrossingOffsetter::CreateSampleOffsetToNearestZCross(const AudioFile &input,
                                                                   const AudioDuration &search_size,
                                                                   const bool append_skipped_frames_on_end) {
    const auto search_frames = search_size.GetDurationAsFrames(input.sample_rate, input.NumFrames());
    std::cout << "Searching " << search_frames << " frames for a zero-crossing\n";

    AudioFile result {};
    result.sample_rate = input.sample_rate;
    result.num_channels = input.num_channels;

    const auto new_start_frame =
        FindFrameNearestToZeroInBuffer(input.interleaved_samples, search_frames, input.num_channels);
    if (new_start_frame == 0) {
        std::cout << "No start frame change needed\n";
        result.interleaved_samples = input.interleaved_samples;
        return result;
    }

    std::cout << "Found best approx zero-crossing frame at position " << new_start_frame << "\n";

    auto interleaved_samples_new_start_it = input.interleaved_samples.begin() + new_start_frame * 2;
    std::vector<double> new_interleaved_samples {interleaved_samples_new_start_it,
                                                 input.interleaved_samples.end()};
    if (append_skipped_frames_on_end) {
        new_interleaved_samples.insert(new_interleaved_samples.end(), input.interleaved_samples.begin(),
                                       interleaved_samples_new_start_it);
        REQUIRE(new_interleaved_samples.size() == input.interleaved_samples.size());
    }

    result.interleaved_samples = new_interleaved_samples;
    return result;
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
}
