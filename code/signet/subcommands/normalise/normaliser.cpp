#include "normaliser.h"

#include "doctest.hpp"

CLI::App *Normaliser::CreateSubcommandCLI(CLI::App &app) {
    auto norm = app.add_subcommand("norm", "Normalise a sample to a certain level");
    norm->add_option("target-decibels", m_target_decibels,
                     "The target level in decibels to convert the sample(s) to")
        ->required()
        ->check(CLI::Range(-200, 0));
    norm->add_flag("-c,--common-gain", m_use_common_gain,
                   "When using on a directory, amplifiy all the samples by the same amount");
    norm->add_flag("--rms", m_use_rms, "Use RMS normalisation instead of peak");
    return norm;
}

bool Normaliser::Process(AudioFile &input) {
    switch (m_current_stage) {
        case ProcessingStage::FindingCommonGain: {
            ReadFileForCommonGain(input);
            return false;
        }
        case ProcessingStage::ApplyingGain: {
            return PerformNormalisation(input);
        }
        default: REQUIRE(0);
    }
    return false;
}

void Normaliser::Run(SignetInterface &signet) {
    if (m_use_rms) {
        m_processor = std::make_unique<RMSGainCalculator>();
    } else {
        m_processor = std::make_unique<PeakGainCalculator>();
    }

    if (signet.IsProcessingMultipleFiles() && m_use_common_gain) {
        m_current_stage = ProcessingStage::FindingCommonGain;
        signet.ProcessAllFiles(*this);
    }

    m_current_stage = ProcessingStage::ApplyingGain;
    signet.ProcessAllFiles(*this);
}

bool Normaliser::PerformNormalisation(AudioFile &input_audio) const {
    if (!m_use_common_gain) {
        m_processor->RegisterBufferMagnitudes(input_audio);
    }
    const double gain = m_processor->GetGain(DBToAmp(m_target_decibels));

    std::cout << "Applying a gain of " << gain << "\n";

    for (auto &s : input_audio.interleaved_samples) {
        s *= gain;
    }

    return true;
}

void Normaliser::ReadFileForCommonGain(const AudioFile &audio) {
    m_processor->RegisterBufferMagnitudes(audio);
}
