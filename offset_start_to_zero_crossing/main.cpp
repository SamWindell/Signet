#include <algorithm>
#include <assert.h>
#include <cmath>
#include <float.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "CLI11.hpp"
#include "common.h"
#include "dr_flac.h"
#include "dr_wav.h"

size_t FindFrameNearestToZeroInBuffer(const float *buffer, const size_t num_frames) {
    float minimum_range = FLT_MAX;
    size_t index_of_min = 0;
    for (size_t frame = 0; frame < num_frames; ++frame) {
        const float frame_distance = std::abs(buffer[frame * 2]) + std::abs(buffer[frame * 2 + 1]);
        if (frame_distance < minimum_range) {
            minimum_range = frame_distance;
            index_of_min = frame;
        }
    }
    return index_of_min;
}

AudioFile CreateNewFileWithStartingZeroCrossing(const AudioFile &input,
                                                size_t num_zero_crossing_search_frames,
                                                const bool append_skipped_frames_on_end) {
    std::cout << "Searching " << num_zero_crossing_search_frames << " frames for a zero-crossing\n";

    AudioFile result {};
    result.sample_rate = input.sample_rate;
    result.num_channels = input.num_channels;

    num_zero_crossing_search_frames = std::min(num_zero_crossing_search_frames, input.NumFrames());

    const auto new_start_frame =
        FindFrameNearestToZeroInBuffer(input.interleaved_samples.data(), num_zero_crossing_search_frames);
    if (new_start_frame == 0) {
        std::cout << "No start frame change needed\n";
        result.interleaved_samples = input.interleaved_samples;
        return result;
    }

    std::cout << "Found best approx zero-crossing frame at position " << new_start_frame << "\n";

    auto interleaved_samples_new_start_it = input.interleaved_samples.begin() + new_start_frame * 2;
    std::vector<float> new_interleaved_samples {interleaved_samples_new_start_it,
                                                input.interleaved_samples.end()};
    if (append_skipped_frames_on_end) {
        new_interleaved_samples.insert(new_interleaved_samples.end(), input.interleaved_samples.begin(),
                                       interleaved_samples_new_start_it);
        assert(new_interleaved_samples.size() == input.interleaved_samples.size());
    }

    result.interleaved_samples = new_interleaved_samples;
    return result;
}

int main(const int argc, const char *argv[]) {
    CLI::App app {"Offset the start of a FLAC or WAV file to the nearest approximate zero crossing"};

    std::string input_filename;
    std::string output_filename;
    bool append_skipped_frames_on_end = false;
    size_t num_zero_crossing_search_frames = 44100;

    app.add_option("input-wave-or-flac-filename", input_filename, "the file to read from")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("output-wave-filename", output_filename, "the file to write to")->required();
    app.add_flag(
        "-a,--append-skipped", append_skipped_frames_on_end,
        "append the frames offsetted to the end of the file - useful when the sample is a seamless loop");
    app.add_option(
           "-n,--search-frames", num_zero_crossing_search_frames,
           "the maximum number of frames from the start of the sample to search for the zero crossing in")
        ->required();

    CLI11_PARSE(app, argc, argv);

    const auto audio_file = ReadAudioFile(input_filename);
    const auto new_audio_file = CreateNewFileWithStartingZeroCrossing(
        audio_file, num_zero_crossing_search_frames, append_skipped_frames_on_end);
    if (!WriteWaveFile(output_filename, new_audio_file)) {
        FatalErrorWithNewLine("could not write the wave file ", output_filename);
    }
    std::cout << "Successfully wrote file " << output_filename
              << (append_skipped_frames_on_end ? " with appending\n" : " without appending\n");

    return 0;
}
