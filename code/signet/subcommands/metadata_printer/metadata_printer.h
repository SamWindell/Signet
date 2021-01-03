#pragma once

#include "subcommand.h"

class MetadataPrinter : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(const tcb::span<EditTrackedAudioFile> files) override;
    std::string GetName() const override { return "MetadataPrinter"; }
};