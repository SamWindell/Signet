#pragma once
#include <memory>

#include "common.h"
#include "gain_calculators.h"
#include "signet_interface.h"

class Normaliser final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    bool ProcessAudio(AudioFile &input, const std::string_view filename) override;
    void Run(SubcommandProcessor &processor) override;

  private:
    bool PerformNormalisation(AudioFile &input_audio) const;
    bool ReadFileForCommonGain(const AudioFile &audio);

    enum class ProcessingStage {
        FindingCommonGain,
        ApplyingGain,
    };
    ProcessingStage m_current_stage {};
    bool m_successfully_found_common_gain {};

    std::unique_ptr<NormalisationGainCalculator> m_processor {};

    bool m_use_common_gain = false;
    double m_target_decibels = 0.0;
    bool m_use_rms = false;
};
