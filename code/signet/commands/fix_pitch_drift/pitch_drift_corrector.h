#pragma once

#include <vector>

#include "span.hpp"

#include "audio_data.h"

struct AnalysisChunk {
    usize frame_start {};
    int frame_size {};
    double detected_pitch {};

    bool detected_pitch_outlier {};
    bool ignore_tuning {};
    double target_pitch {};

    double pitch_ratio_for_print {};
};

class PitchDriftCorrector {
  public:
    PitchDriftCorrector(const AudioData &data);
    bool CanFileBePitchCorrected() const;
    void ProcessFile(AudioData &data);

  private:
    void FixObviousOutliers();
    void MarkInvalidAnalysisChunks();
    void MarkRegionsToIgnore();
    double TargetPitchForAnalysisChunkRegion(tcb::span<const AnalysisChunk> chunks);
    void MarkTargetPitches();
    std::vector<double> CalculatePitchCorrectedInterleavedSamples(const AudioData &data);

    std::vector<AnalysisChunk> m_chunks;
};
