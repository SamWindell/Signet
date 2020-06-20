#pragma once

#include "subcommand.h"

class Highpass final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(const tcb::span<EditTrackedAudioFile> files) override;

  private:
    double m_cutoff;
};
