#pragma once

#include <optional>

#include "command.h"

class FolderiseCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Folderise"; }
    bool AllowsOutputFolder() const override { return false; }

  private:
    std::string m_filename_pattern;
    std::string m_out_folder;
};
