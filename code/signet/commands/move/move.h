#pragma once

#include "command.h"

class MoveCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Move"; }
    bool AllowsOutputFolder() const override { return false; }

  private:
    fs::path m_destination_dir;
};