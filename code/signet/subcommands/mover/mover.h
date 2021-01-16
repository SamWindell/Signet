#pragma once

#include "subcommand.h"

class Mover final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Mover"; }

  private:
    fs::path m_destination_dir;
};