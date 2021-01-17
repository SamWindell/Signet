#pragma once
#include <optional>

#include "filesystem.hpp"

#include "audio_data.h"

std::optional<AudioData> ReadAudioFile(const fs::path &filename);
bool WriteAudioFile(const fs::path &filename,
                    const AudioData &audio_data,
                    const std::optional<unsigned> new_bits_per_sample = {});

bool CanFileBeConvertedToBitDepth(AudioFileFormat file, unsigned bit_depth);
bool IsPathReadableAudioFile(const fs::path &path);
std::string GetLowercaseExtension(AudioFileFormat file);
