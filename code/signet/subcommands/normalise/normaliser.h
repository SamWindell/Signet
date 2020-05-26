#pragma once
#include <cassert>
#include <memory>

#include "common.h"
#include "gain_calculators.h"
#include "signet_interface.h"

class Normaliser final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    std::optional<AudioFile> Process(const AudioFile &input, ghc::filesystem::path &output_filename) override;
    void Run(SignetInterface &audio_util) override;

  private:
    AudioFile PerformNormalisation(const AudioFile &input_audio) const;
    void ReadFileForCommonGain(const AudioFile &audio);

    enum class ProcessingStage {
        FindingCommonGain,
        ApplyingGain,
    };
    ProcessingStage m_current_stage {};

    std::unique_ptr<NormalisationGainCalculator> m_processor {};

    bool m_use_common_gain = false;
    float m_target_decibels = 0.0f;
    bool m_use_rms = false;
};
