#pragma once
#include <memory>
#include <optional>

#include "common.h"
#include "gain_calculators.h"
#include "signet_interface.h"

class Normaliser final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(const tcb::span<EditTrackedAudioFile> files) override;
    std::string GetName() override { return "Normaliser"; }

  private:
    bool PerformNormalisation(AudioData &input_audio) const;
    bool ReadFileForCommonGain(const AudioData &audio);

    bool m_using_common_gain {};

    std::unique_ptr<NormalisationGainCalculator> m_processor {};

    std::optional<double> m_norm_mix {};
    bool m_normalise_independently = false;
    double m_target_decibels = 0.0;
    bool m_use_rms = false;
};
