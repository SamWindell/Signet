#pragma once
#include <memory>
#include <optional>

#include "common.h"
#include "gain_calculators.h"
#include "signet_interface.h"

class NormaliseCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Normalise"; }

  private:
    std::optional<double> m_norm_mix {};
    bool m_normalise_independently = false;
    bool m_normalise_channels_separately = false;
    double m_target_decibels = 0.0;
    bool m_use_rms = false;
};
