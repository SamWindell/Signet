#pragma once

#include "command.h"

class SeamlessLoopCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "SeamlessLoop"; }

  private:
    double m_crossfade_percent;
};
