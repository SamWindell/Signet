#pragma once
#include "command.h"

class NormaliseCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Normalise"; }

  private:
    double m_norm_mix_percent {100.0};
    double m_norm_channel_mix_percent {100.0};
    double m_crest_factor_scaling {0.0};
    bool m_normalise_independently = false;
    bool m_normalise_channels_separately = false;
    double m_target_decibels = 0.0;
    bool m_use_rms = false;
};
