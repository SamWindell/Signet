#pragma once

#include <string>

#include "command.h"

class MirReportCommand : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "MirReport"; }

    bool AllowsOutputFolder() const override { return false; }
    bool AllowsSingleOutputFile() const override { return false; }
    bool IsReadOnly() const override { return true; }
};
