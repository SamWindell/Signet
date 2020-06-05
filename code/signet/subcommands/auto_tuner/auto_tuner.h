#pragma once

#include "subcommand.h"

class AutoTuner final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    bool Process(AudioFile &input, const std::string_view filename) override;
    void Run(SubcommandProcessor &processor) override { processor.ProcessAllFiles(*this); }
};
