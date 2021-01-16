#pragma once

#include "subcommand.h"

class AutoTuner final : public Subcommand {
  public:
    std::string GetName() const override { return "AutoTuner"; }
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
};
