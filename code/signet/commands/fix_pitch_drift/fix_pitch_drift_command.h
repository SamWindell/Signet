#pragma once

#include "command.h"
#include "expected_midi_pitch.h"
#include "identical_processing_set.h"
#include "midi_pitches.h"

class FixPitchDriftCommand final : public Command {
  public:
    std::string GetName() const override { return "FixPitchDrift"; }
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;

  private:
    IdenticalProcessingSet m_identical_processing_set;
    double m_chunk_length_milliseconds {60.0};
    bool m_print_csv {false};
    ExpectedMidiPitch m_expected_midi_pitch;
};
