#pragma once

#include <string_view>
#include <vector>

#include "span.hpp"

#include "audio_data.h"
#include "midi_pitches.h"

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
    PitchDriftCorrector(const AudioData &data,
                        std::string_view message_heading,
                        const fs::path &file_name,
                        double chunk_length_milliseconds,
                        bool print_csv);
    bool CanFileBePitchCorrected() const;
    bool ProcessFile(AudioData &data, std::optional<MIDIPitch> expected_midi_pitch);

  private:
    static constexpr bool k_brute_force_fix_octave_errors = false;

    void MarkOutlierChunks();
    void MarkRegionsToIgnore();
    static double FindTargetPitchForChunkRegion(tcb::span<const AnalysisChunk> chunks);
    int MarkTargetPitches();
    std::vector<double> CalculatePitchCorrectedInterleavedSamples(const AudioData &data);

    void PrintChunkCSV() const;

    std::string m_message_heading;
    fs::path m_file_name;
    double m_chunk_length_milliseconds; // near 50ms is best
    unsigned m_sample_rate;
    bool m_print_csv;
    std::vector<AnalysisChunk> m_chunks;
};
