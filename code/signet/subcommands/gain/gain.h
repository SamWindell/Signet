#pragma once

#include "subcommand.h"

class GainAmount {
  public:
    enum class Unit {
        Decibels,
        Percent,
    };

    GainAmount() {}
    GainAmount(std::string str);
    double GetMultiplier() const;

  private:
    Unit m_unit {};
    double m_value {};
};

class GainCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Gain"; }

  private:
    GainAmount m_gain;
};
