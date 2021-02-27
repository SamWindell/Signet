#pragma once

#include <vector>

#include "span.hpp"

#include "audio_data.h"

struct AnalysisChunk {
    usize frame_start {};
    int frame_size {};
    double detected_pitch {};

    bool is_detected_pitch_outlier {};
    bool ignore_tuning {};
    double target_pitch {};

    double pitch_ratio_for_print {};
};

class PitchDriftCorrector {
  public:
    PitchDriftCorrector(const AudioData &data, std::string_view print_heading);
    bool CanFileBePitchCorrected() const;
    bool ProcessFile(AudioData &data);

  private:
    static constexpr double k_chunk_length_milliseconds = 50;

    void FixObviousDetectedPitchOutliers();
    void MarkOutlierChunks();
    void MarkRegionsToIgnore();
    static double FindTargetPitchForChunkRegion(tcb::span<const AnalysisChunk> chunks);
    int MarkTargetPitches();
    std::vector<double> CalculatePitchCorrectedInterleavedSamples(const AudioData &data);

    std::string m_print_heading;
    std::vector<AnalysisChunk> m_chunks;
};
