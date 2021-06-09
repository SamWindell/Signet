#pragma once

#include "command.h"

class AutoTuneCommand final : public Command {
  public:
    std::string GetName() const override { return "AutoTune"; }
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;

  private:
    std::vector<std::string> m_sample_set_args;
};
