#pragma once

#include "subcommand.h"

class AutoTuneCommand final : public Command {
  public:
    std::string GetName() const override { return "AutoTune"; }
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
};
