#pragma once

#include "subcommand.h"

class SeamlessLooper final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "SeamlessLooper"; }

  private:
    double m_crossfade_percent;
};
