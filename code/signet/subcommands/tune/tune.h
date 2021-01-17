#pragma once

#include "subcommand.h"

class TuneCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Tune"; }

  private:
    double m_tune_cents {};
};
