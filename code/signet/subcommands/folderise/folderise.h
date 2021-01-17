#pragma once

#include <optional>

#include "subcommand.h"

class FolderiseCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Folderise"; }

  private:
    std::string m_filename_pattern;
    std::string m_out_folder;
};
