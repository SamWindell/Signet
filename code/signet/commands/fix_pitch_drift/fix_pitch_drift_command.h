#pragma once

#include "command.h"

class FixPitchDriftCommand final : public Command {
  public:
    std::string GetName() const override { return "FixPitchDrift"; }
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
};
