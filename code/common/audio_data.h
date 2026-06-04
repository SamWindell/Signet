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

    struct PitchDetectionResult {
        double hz;
        double confidence; // 0..1, based on agreement across octave-shifted re-detections
    };
    std::optional<PitchDetectionResult> DetectPitchWithConfidence() const;
    std::optional<double> DetectPitch() const;

    struct PitchTrackEntry {
        double time_seconds; // centre of the chunk
        double hz;           // 0 if unvoiced
        double rms;
    };
    // Raw per-chunk pitch track from the underlying detector. Chunks of `chunk_seconds` are
    // taken contiguously across the file; the returned hz is 0 for chunks the detector marked
    // unvoiced. Used both as input to DetectPitchWithConfidence and exposed for time-resolved
    // analysis (MIR reports, pitch-over-time visualisations).
    std::vector<PitchTrackEntry> DetectPitchTrack(double chunk_seconds = 0.1) const;

    bool IsSilent() const;

    std::vector<double> MixDownToMono() const;

    //
    //
    void FramesWereRemovedFromStart(size_t num_frames);
    void FramesWereRemovedFromEnd();
    void AudioDataWasStretched(double stretch_factor);
    void AudioDataWasReversed();

  private:
    void PrintMetadataRemovalWarning(std::string_view metadata_name);
};
