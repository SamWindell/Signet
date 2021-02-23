#pragma once

#include "command.h"

class RealTimeAutoTuneCommand final : public Command {
  public:
    std::string GetName() const override { return "RealTimeAutoTune"; }
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
};
