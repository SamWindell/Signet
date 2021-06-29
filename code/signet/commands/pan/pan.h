#pragma once

#include "command.h"

struct PanUnit {
    PanUnit() {}
    PanUnit(std::string str);
    operator double() const { return value; }

    double value {};
};

class PanCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Pan"; }

  private:
    PanUnit m_pan {};
};
