#pragma once

#include "subcommand.h"

class AutoTuneSubcommand final : public Subcommand {
  public:
    std::string GetName() const override { return "AutoTuneSubcommand"; }
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
};
