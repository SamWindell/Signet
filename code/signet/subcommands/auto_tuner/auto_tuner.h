#pragma once

#include "subcommand.h"

class AutoTuner final : public Subcommand {
  public:
    std::string GetName() override { return "AutoTuner"; }
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(const tcb::span<EditTrackedAudioFile> files) override;
};
