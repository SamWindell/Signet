#pragma once

#include <cassert>
#include <vector>

#include "FLAC/metadata.h"

#include "common.h"
#include "metadata.h"
#include "types.h"

enum class AudioFileFormat {
    Wav,
    Flac,
};

bool ApproxEqual(double a, double b, double epsilon);

struct AudioData {
    std::vector<double> interleaved_samples {};
    unsigned num_channels {};
    unsigned sample_rate {};
    unsigned bits_per_sample = 24;
    AudioFileFormat format {AudioFileFormat::Wav};

    Metadata metadata {};

    // We store these file-format-specific bits of metadata because not all types of metadata are handled by
    // the generic Metadata struct and therefore it still might contain data that we want to write back to the
    // file.
    WaveMetadata wave_metadata {};
    std::vector<std::shared_ptr<FLAC__StreamMetadata>> flac_metadata {};

    //
    //
    bool IsEmpty() const { return interleaved_samples.empty(); }
    size_t NumFrames() const;
    double &GetSample(unsigned channel, size_t frame);
    const double &GetSample(unsigned channel, size_t frame) const;

    //
    //
    void MultiplyByScalar(const double amount);
    void MultiplyByScalar(unsigned channel, const double amount);
    void AddOther(const AudioData &other);
    void Resample(double new_sample_rate);
    void ChangePitch(double cents);
    std::optional<double> DetectPitch() const;
    bool IsSilent() const;

    std::vector<double> MixDownToMono() const;

    //
    //
    void FramesWereRemovedFromStart(size_t num_frames);
    void FramesWereRemovedFromEnd();
    void AudioDataWasStretched(double stretch_factor);

  private:
    void PrintMetadataRemovalWarning(std::string_view metadata_name);
};
