#pragma once

#include "command.h"
#include "identical_processing_set.h"

class FixPitchDriftCommand final : public Command {
  public:
    std::string GetName() const override { return "FixPitchDrift"; }
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;

  private:
    IdenticalProcessingSet m_identical_processing_set;
    double m_chunk_length_milliseconds {60.0};
    bool m_print_csv {false};
};
