#pragma once

#include "command.h"

class FixPitchDriftCommand final : public Command {
  public:
    std::string GetName() const override { return "FixPitchDrift"; }
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;

  private:
    double m_chunk_length_milliseconds {60.0};
    bool m_print_csv {false};
};
