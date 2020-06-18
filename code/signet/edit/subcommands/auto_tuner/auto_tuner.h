#pragma once

#include "edit/subcommand.h"

class AutoTuner final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    bool ProcessAudio(AudioFile &input, const std::string_view filename) override;
    void Run(SubcommandHost &processor) override { processor.ProcessAllFiles(*this); }
};
