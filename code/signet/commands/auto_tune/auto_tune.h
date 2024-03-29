#pragma once

#include "command.h"
#include "expected_midi_pitch.h"
#include "identical_processing_set.h"

class AutoTuneCommand final : public Command {
  public:
    std::string GetName() const override { return "AutoTune"; }
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;

  private:
    IdenticalProcessingSet m_identical_processing_set;
    ExpectedMidiPitch m_expected_midi_pitch;
};
