#include <algorithm>
#include <assert.h>
#include <cmath>
#include <float.h>
#include <stdio.h>
#include <string>

#include "audio_duration.h"
#include "audio_file.h"
#include "audio_util_interface.h"
#include "common.h"

struct ZeroCrossingOffseter final : public Processor {
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

    std::optional<AudioFile> Process(const AudioFile &input,
                                     ghc::filesystem::path &output_filepath) override {
        const auto search_frames = m_search_size.GetDurationAsFrames(input.sample_rate, input.NumFrames());
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
        if (m_append_skipped_frames_on_end) {
            new_interleaved_samples.insert(new_interleaved_samples.end(), input.interleaved_samples.begin(),
                                           interleaved_samples_new_start_it);
            assert(new_interleaved_samples.size() == input.interleaved_samples.size());
        }

        result.interleaved_samples = new_interleaved_samples;
        return result;
    }

    void Run(AudioUtilInterface &util) override { util.ProcessAllFiles(*this); }

    void AddCLI(CLI::App &app) override {
        auto zcross = app.add_subcommand(
            "zcross-offset", "Offset the start of a FLAC or WAV file to the nearest zero-crossing");
        zcross->add_flag(
            "-a,--append-skipped", m_append_skipped_frames_on_end,
            "Append the frames offsetted to the end of the file - useful when the sample is a seamless loop");

        zcross
            ->add_option("search_size", m_search_size,
                         "The duration from the start of the sample to search for the zero crossing in")
            ->required()
            ->check(AudioDuration::ValidateString, AudioDuration::ValidatorDescription());
    }

  private:
    bool m_append_skipped_frames_on_end = false;
    AudioDuration m_search_size {AudioDuration::Unit::Seconds, 1.0f};
};
