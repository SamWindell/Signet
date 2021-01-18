#pragma once

#include "command.h"

class PrintInfoCommand : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "PrintInfo"; }
};