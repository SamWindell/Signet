#pragma once
#include <optional>
#include <vector>

#include "doctest.hpp"
#include "filesystem.hpp"

enum class AudioFileFormat {
    Wave,
    Flac,
};

struct AudioFile {
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

    std::vector<double> interleaved_samples {};
    unsigned num_channels {};
    unsigned sample_rate {};
    unsigned bits_per_sample = 24;
    AudioFileFormat format {AudioFileFormat::Wave};
};

std::optional<AudioFile> ReadAudioFile(const fs::path &filename);
bool WriteAudioFile(const fs::path &filename,
                    const AudioFile &audio_file,
                    const std::optional<unsigned> new_bits_per_sample = {});

bool CanFileBeConvertedToBitDepth(const AudioFile &file, unsigned bit_depth);
