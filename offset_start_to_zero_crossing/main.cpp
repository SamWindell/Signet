#include <algorithm>
#include <assert.h>
#include <cmath>
#include <float.h>
#include <stdio.h>
#include <string>

#include "CLI11.hpp"
#include "common.h"
#include "dr_flac.h"
#include "dr_wav.h"
#include "filesystem.hpp"

struct ZeroCrossingOffseter {
    static size_t FindFrameNearestToZeroInBuffer(const float *interleaved_buffer,
                                                 const size_t num_frames,
                                                 const unsigned num_channels) {
        float minimum_range = FLT_MAX;
        size_t index_of_min = 0;
        for (size_t frame = 0; frame < num_frames; ++frame) {
            float frame_distance = 0;
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

    AudioFile CreateAudioFileWithNewZCross(const AudioFile &input) const {
        const auto search_frames = std::min(num_zero_crossing_search_frames, input.NumFrames());
        std::cout << "Searching " << search_frames << " frames for a zero-crossing\n";

        AudioFile result {};
        result.sample_rate = input.sample_rate;
        result.num_channels = input.num_channels;

        const auto new_start_frame = FindFrameNearestToZeroInBuffer(input.interleaved_samples.data(),
                                                                    search_frames, input.num_channels);
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

    void ProcessFile(const ghc::filesystem::path &input_filepath,
                     ghc::filesystem::path output_filepath) const {
        if (output_filepath.empty()) {
            output_filepath = input_filepath;
            if (!delete_input_files) {
                output_filepath.replace_extension();
                output_filepath += "(edited)";
            }
            output_filepath.replace_extension(".wav");
        }
        assert(!input_filepath.empty());

        if (const auto audio_file = ReadAudioFile(input_filepath)) {
            const auto new_audio_file = CreateAudioFileWithNewZCross(*audio_file);
            if (!WriteWaveFile(output_filepath, new_audio_file)) {
                FatalErrorWithNewLine("could not write the wave file ", output_filepath);
            }
            std::cout << "Successfully wrote file " << output_filepath
                      << (append_skipped_frames_on_end ? " with appending\n" : " without appending\n");

            if (delete_input_files && input_filepath != output_filepath) {
                std::cout << "Deleting file " << input_filepath << "\n";
                ghc::filesystem::remove(input_filepath);
            }
        }

        std::cout << "\n";
    }

    void ProcessToAutomaticallyNamedOutput(const ghc::filesystem::path &input_filepath) const {
        ProcessFile(input_filepath, {});
    }

    void Process() const {
        if (ghc::filesystem::is_directory(input_filepath)) {
            if (!output_filepath.empty()) {
                FatalErrorWithNewLine(
                    "the input path is a directory, there must be no output filepath - output "
                    "files will be placed adjacent to originals");
            }

            ForEachAudioFileInDirectory(
                input_filepath, recursive_directory_search,
                [this](const ghc::filesystem::path &path) { ProcessToAutomaticallyNamedOutput(path); });
        } else {
            if (recursive_directory_search) {
                WarningWithNewLine("input path is a file, ignoring the recursive flag");
            }
            if (ghc::filesystem::is_directory(output_filepath)) {
                FatalErrorWithNewLine(
                    "output filepath cannot be a directory if the input filepath is a file");
            }
            ProcessFile(input_filepath, output_filepath);
        }
    }

    void CreateCLI(CLI::App &app) {
        app.add_option("input-file-or-directory", input_filepath, "The file or directory to read from")
            ->required()
            ->check(CLI::ExistingPath);
        app.add_option("output-wave-filename", output_filepath,
                       "The filename to write to - only relevant if the input file is not a directory");
        app.add_flag(
            "-a,--append-skipped", append_skipped_frames_on_end,
            "Append the frames offsetted to the end of the file - useful when the sample is a seamless loop");
        app.add_option(
            "-n,--search-frames", num_zero_crossing_search_frames,
            "The maximum number of frames from the start of the sample to search for the zero crossing in");
        app.add_flag("-r,--recursive-directory-search", recursive_directory_search,
                     "Search for files recursively in the given directory");
        app.add_flag("-d,--delete-input-files", delete_input_files,
                     "Delete the input files if the new file was successfully written");
    }

  private:
    bool append_skipped_frames_on_end = false;
    size_t num_zero_crossing_search_frames = 44100;
    bool delete_input_files = false;
    ghc::filesystem::path input_filepath;
    ghc::filesystem::path output_filepath;
    bool recursive_directory_search = false;
};

int main(const int argc, const char *argv[]) {
    ZeroCrossingOffseter zcross_offsetter {};

    CLI::App app {"Offset the start of a FLAC or WAV file to the nearest approximate zero-crossing"};
    zcross_offsetter.CreateCLI(app);
    CLI11_PARSE(app, argc, argv);

    zcross_offsetter.Process();
    return 0;
}
