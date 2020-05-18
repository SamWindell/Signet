#include "common.h"

#define DR_WAV_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#include "dr_wav.h"

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

std::string_view GetExtension(const std::string_view path) {
    const auto index = path.find_last_of('.');
    if (index == std::string::npos) {
        return path;
    }
    return path.substr(index);
}
