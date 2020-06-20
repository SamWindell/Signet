#pragma once
#include <memory>

#include "common.h"
#include "gain_calculators.h"
#include "signet_interface.h"

class Normaliser final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(const tcb::span<EditTrackedAudioFile> files) override;

  private:
    bool PerformNormalisation(AudioData &input_audio) const;
    bool ReadFileForCommonGain(const AudioData &audio);

    bool m_successfully_found_common_gain {};

    std::unique_ptr<NormalisationGainCalculator> m_processor {};

    bool m_normalise_independently = false;
    double m_target_decibels = 0.0;
    bool m_use_rms = false;
};
