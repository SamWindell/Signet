#pragma once

#include "subcommand.h"

class SampleInfoPrinter : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "SampleInfoPrinter"; }
};