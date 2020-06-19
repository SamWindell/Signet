#pragma once
#include <optional>
#include <vector>

#include "doctest.hpp"
#include "filesystem.hpp"

#include "types.h"

enum class AudioFileFormat {
    Wav,
    Flac,
};

struct AudioFile {
    bool IsEmpty() const { return interleaved_samples.empty(); }
    size_t NumFrames() const {
        REQUIRE(num_channels != 0);
        return interleaved_samples.size() / num_channels;
    }
    double &GetSample(unsigned channel, size_t frame) {
        return interleaved_samples[frame * num_channels + channel];
    }
    const double &GetSample(unsigned channel, size_t frame) const {
        return interleaved_samples[frame * num_channels + channel];
    }
    void MultiplyByScalar(const double amount) {
        for (auto &s : interleaved_samples) {
            s *= amount;
        }
    }
    void AddOther(const AudioFile &other) {
        if (other.interleaved_samples.size() > interleaved_samples.size()) {
            interleaved_samples.resize(other.interleaved_samples.size());
        }
        for (usize i = 0; i < other.interleaved_samples.size(); ++i) {
            interleaved_samples[i] += other.interleaved_samples[i];
        }
    }

    std::vector<double> interleaved_samples {};
    unsigned num_channels {};
    unsigned sample_rate {};
    unsigned bits_per_sample = 24;
    AudioFileFormat format {AudioFileFormat::Wav};
};

std::optional<AudioFile> ReadAudioFile(const fs::path &filename);
bool WriteAudioFile(const fs::path &filename,
                    const AudioFile &audio_file,
                    const std::optional<unsigned> new_bits_per_sample = {});

bool CanFileBeConvertedToBitDepth(AudioFileFormat file, unsigned bit_depth);
bool IsAudioFileReadable(const fs::path &path);
std::string GetLowercaseExtension(AudioFileFormat file);
