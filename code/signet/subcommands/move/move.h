#pragma once

#include "subcommand.h"

class MoveCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Move"; }

  private:
    fs::path m_destination_dir;
};