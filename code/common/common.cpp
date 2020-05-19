#include "common.h"

#define DR_WAV_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#include "dr_wav.h"

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

bool WriteWaveFile(const ghc::filesystem::path &path, const AudioFile &audio_file) {
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

template <typename DirectoryIterator>
void ForEachAudioFileInDirectory(const std::string &directory,
                                 std::function<void(const ghc::filesystem::path &)> callback) {
    std::vector<ghc::filesystem::path> paths;
    for (const auto &entry : DirectoryIterator(directory)) {
        const auto &path = entry.path();
        const auto ext = path.extension();
        if (ext == ".flac" || ext == ".wav") {
            paths.push_back(path);
        }
    }

    // We do this in a separate loop because the callback might write a file. We do not want to then process
    // it again.
    for (const auto &path : paths) {
        callback(path);
    }
    std::cout << "Processed " << paths.size() << (paths.size() == 1 ? " file\n\n" : " files\n\n");
}

void ForEachAudioFileInDirectory(const std::string &directory,
                                 const bool recursive,
                                 std::function<void(const ghc::filesystem::path &)> callback) {
    if (recursive) {
        ForEachAudioFileInDirectory<ghc::filesystem::recursive_directory_iterator>(directory, callback);
    } else {
        ForEachAudioFileInDirectory<ghc::filesystem::directory_iterator>(directory, callback);
    }
}

float DBToAmp(const float d) { return std::pow(10.0f, d / 20.0f); }
