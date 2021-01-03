#pragma once

#include <cassert>
#include <vector>

#include "FLAC/metadata.h"
#include "span.hpp"

#include "common.h"
#include "metadata.h"
#include "types.h"

enum class AudioFileFormat {
    Wav,
    Flac,
};

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
    size_t NumFrames() const {
        assert(num_channels != 0);
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
    void AddOther(const AudioData &other) {
        if (other.interleaved_samples.size() > interleaved_samples.size()) {
            interleaved_samples.resize(other.interleaved_samples.size());
        }
        for (usize i = 0; i < other.interleaved_samples.size(); ++i) {
            interleaved_samples[i] += other.interleaved_samples[i];
        }
    }

    void FramesWereRemovedFromStart(size_t num_frames) {
        if (metadata.HandleStartFramesRemovedForType<MetadataItems::Region>(metadata.regions, num_frames)) {
            PrintMetadataRemovalWarning("regions");
        }
        if (metadata.HandleStartFramesRemovedForType<MetadataItems::Marker>(metadata.markers, num_frames)) {
            PrintMetadataRemovalWarning("markers");
        }
        if (metadata.HandleStartFramesRemovedForType<MetadataItems::Loop>(metadata.loops, num_frames)) {
            PrintMetadataRemovalWarning("loops");
        }
    }

    void FramesWereRemovedFromEnd() {
        if (metadata.HandleEndFramesRemovedForType<MetadataItems::Region>(metadata.regions, NumFrames())) {
            PrintMetadataRemovalWarning("regions");
        }
        if (metadata.HandleEndFramesRemovedForType<MetadataItems::Loop>(metadata.loops, NumFrames())) {
            PrintMetadataRemovalWarning("loops");
        }
        bool markers_removed = false;
        for (auto it = metadata.markers.begin(); it != metadata.markers.end();) {
            if (it->start_frame >= NumFrames()) {
                it = metadata.markers.erase(it);
                markers_removed = true;
            } else {
                ++it;
            }
        }
        if (markers_removed) {
            PrintMetadataRemovalWarning("markers");
        }
    }

    void AudioDataWasStretched(double stretch_factor) {
        for (auto &r : metadata.regions) {
            r.start_frame = (size_t)(r.start_frame * stretch_factor);
            r.num_frames = (size_t)(r.num_frames * stretch_factor);
            assert(r.start_frame < NumFrames());
            assert(r.start_frame + r.num_frames <= NumFrames());
        }
        for (auto &r : metadata.markers) {
            r.start_frame = (size_t)(r.start_frame * stretch_factor);
            assert(r.start_frame < NumFrames());
        }
        for (auto &r : metadata.loops) {
            r.start_frame = (size_t)(r.start_frame * stretch_factor);
            r.num_frames = (size_t)(r.num_frames * stretch_factor);
            assert(r.start_frame < NumFrames());
            assert(r.start_frame + r.num_frames <= NumFrames());
        }
    }

  private:
    void PrintMetadataRemovalWarning(std::string_view metadata_name) {
        WarningWithNewLine("Signet",
                           "One or more metadata {} were removed from the file because the file changed size",
                           metadata_name);
    }
};
