#include "audio_file.h"

#include <iostream>

#define DR_WAV_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#include "doctest.hpp"
#include "dr_flac.h"
#include "dr_wav.h"
#include "flac/all.h"
#include "flac/stream_encoder.h"

#include "common.h"

std::optional<AudioFile> ReadAudioFile(const ghc::filesystem::path &path) {
    std::cout << "Reading file " << path << "\n";
    AudioFile result {};
    const auto ext = path.extension();
    if (ext == ".wav") {
        std::unique_ptr<drwav, decltype(&drwav_close)> wav(drwav_open_file(path.generic_string().data()),
                                                           &drwav_close);
        if (!wav) {
            WarningWithNewLine("could not open the WAV file ", path);
            return {};
        }

        result.num_channels = wav->channels;
        result.sample_rate = wav->sampleRate;
        result.interleaved_samples.resize(wav->totalPCMFrameCount * wav->channels);
        const auto frames_read =
            drwav_read_pcm_frames_f32(wav.get(), wav->totalPCMFrameCount, result.interleaved_samples.data());
        if (frames_read != wav->totalPCMFrameCount) {
            WarningWithNewLine("failed to get all the frames from file ", path);
            return {};
        }
    } else if (ext == ".flac") {
        std::unique_ptr<drflac, decltype(&drflac_close)> flac(
            drflac_open_file(path.generic_string().data(), nullptr), &drflac_close);
        if (!flac) {
            WarningWithNewLine("could not open the FLAC file ", path);
            return {};
        }

        result.num_channels = flac->channels;
        result.sample_rate = flac->sampleRate;
        result.interleaved_samples.resize(flac->totalPCMFrameCount * flac->channels);
        const auto frames_read = drflac_read_pcm_frames_f32(flac.get(), flac->totalPCMFrameCount,
                                                            result.interleaved_samples.data());
        if (frames_read != flac->totalPCMFrameCount) {
            WarningWithNewLine("failed to get all the frames from file ", path);
            return {};
        }
    } else {
        WarningWithNewLine("file ", path, " is not a WAV or a FLAC");
        return {};
    }
    return result;
}

static bool WriteWaveFile(const ghc::filesystem::path &path, const AudioFile &audio_file) {
    drwav_data_format format {};
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = audio_file.num_channels;
    format.sampleRate = audio_file.sample_rate;
    format.bitsPerSample = sizeof(float) * 8;
    drwav *wav = drwav_open_file_write(path.generic_string().data(), &format);
    if (!wav) return false;

    const auto num_written =
        drwav_write_pcm_frames(wav, audio_file.NumFrames(), audio_file.interleaved_samples.data());
    if (num_written != audio_file.NumFrames()) return false;

    drwav_close(wav);
    return true;
}

static bool WriteFlacFile(const ghc::filesystem::path &filename, const AudioFile &audio_file) {
    constexpr auto bits_per_sample = 16;

    std::unique_ptr<FLAC__StreamEncoder, decltype(&FLAC__stream_encoder_delete)> encoder {
        FLAC__stream_encoder_new(), &FLAC__stream_encoder_delete};
    if (!encoder) return false;

    FLAC__stream_encoder_set_channels(encoder.get(), audio_file.num_channels);
    FLAC__stream_encoder_set_bits_per_sample(encoder.get(), bits_per_sample);
    FLAC__stream_encoder_set_sample_rate(encoder.get(), audio_file.sample_rate);
    FLAC__stream_encoder_set_total_samples_estimate(encoder.get(), audio_file.interleaved_samples.size());

    std::unique_ptr<FILE, decltype(&fclose)> f {std::fopen(filename.generic_string().data(), "w+b"), &fclose};
    if (!f) return false;

    if (const auto o = FLAC__stream_encoder_init_FILE(encoder.get(), f.get(), nullptr, nullptr);
        o != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        return false;
    }

    std::vector<s32> int32_buffer;
    int32_buffer.reserve(audio_file.interleaved_samples.size());
    switch (bits_per_sample) {
        case 16: {
            for (const auto s : audio_file.interleaved_samples) {
                if (s < 0) {
                    int32_buffer.push_back(static_cast<s32>(s * 32768));
                } else {
                    int32_buffer.push_back(static_cast<s32>(s * 32767));
                }
            }
            break;
        }
        default: REQUIRE(false);
    }

    if (!FLAC__stream_encoder_process_interleaved(encoder.get(), int32_buffer.data(),
                                                  (unsigned)audio_file.NumFrames())) {
        return false;
    }

    FLAC__stream_encoder_finish(encoder.get());
    return true;
}

bool WriteAudioFile(const ghc::filesystem::path &filename, const AudioFile &audio_file) {
    const auto ext = filename.extension();
    if (ext == ".flac") {
        return WriteFlacFile(filename, audio_file);
    } else if (ext == ".wav") {
        return WriteWaveFile(filename, audio_file);
    }
    return false;
}
