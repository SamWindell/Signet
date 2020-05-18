#include <algorithm>
#include <cmath>
#include <float.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "CLI11.hpp"
#include "common.h"
#include "dr_flac.h"
#include "dr_wav.h"

struct AudioFile {
    size_t NumFrames() const { return interleaved_samples.size() / num_channels; }

    std::vector<float> interleaved_samples;
    unsigned num_channels;
    unsigned sample_rate;
};

AudioFile ReadAudioFile(const std::string &filename) {
    AudioFile result {};
    const auto ext = GetExtension(filename);
    if (ext == ".wav") {
        drwav wav;
        if (!drwav_init_file(&wav, filename.data())) {
            FatalErrorWithNewLine("could not open the WAV file ", filename);
        }

        result.num_channels = wav.channels;
        result.sample_rate = wav.sampleRate;
        result.interleaved_samples.resize(wav.totalPCMFrameCount * wav.channels);
        const auto frames_read =
            drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, result.interleaved_samples.data());
        if (frames_read != wav.totalPCMFrameCount) {
            FatalErrorWithNewLine("failed to get all the frames from file ", filename);
        }

        drwav_uninit(&wav);
    } else if (ext == ".flac") {
        auto flac = drflac_open_file(filename.data(), nullptr);
        if (!flac) {
            FatalErrorWithNewLine("could not open the FLAC file ", filename);
        }

        result.num_channels = flac->channels;
        result.sample_rate = flac->sampleRate;
        result.interleaved_samples.resize(flac->totalPCMFrameCount * flac->channels);
        const auto frames_read =
            drflac_read_pcm_frames_f32(flac, flac->totalPCMFrameCount, result.interleaved_samples.data());
        if (frames_read != flac->totalPCMFrameCount) {
            FatalErrorWithNewLine("failed to get all the frames from file ", filename);
        }

        drflac_close(flac);
    } else {
        FatalErrorWithNewLine("file '", filename, "' is not a WAV or a FLAC");
    }
    return result;
}

bool WriteWaveFile(const std::string &filename, const AudioFile &audio_file) {
    drwav_data_format format {};
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = audio_file.num_channels;
    format.sampleRate = audio_file.sample_rate;
    format.bitsPerSample = sizeof(float) * 8;
    drwav *wav = drwav_open_file_write(filename.data(), &format);
    if (!wav) return false;

    const auto num_written =
        drwav_write_pcm_frames(wav, audio_file.NumFrames(), audio_file.interleaved_samples.data());
    if (num_written != audio_file.NumFrames()) return false;

    drwav_close(wav);
    return true;
}

size_t FindFrameNearestToZeroInBuffer(const float *buffer, size_t num_frames) {
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
                                                bool append_skipped_frames_on_end) {
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

int main(int argc, char *argv[]) {
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
    const auto new_audio_file = CreateNewFileWithZeroCrossing(audio_file, num_zero_crossing_search_frames,
                                                              append_skipped_frames_on_end);
    if (!WriteWaveFile(output_filename, new_audio_file)) {
        FatalErrorWithNewLine("could not write the wave file ", output_filename);
    }
    std::cout << "Successfully wrote file " << output_filename
              << (append_skipped_frames_on_end ? " with appending\n" : " without appending\n");

    return 0;
}
