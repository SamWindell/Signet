#include "normaliser.h"

#include "doctest.hpp"

CLI::App *Normaliser::CreateSubcommandCLI(CLI::App &app) {
    auto norm = app.add_subcommand(
        "norm",
        "Normaliser: sets the peak amplitude to a certain level. When this is used on multiple files, each "
        "file is attenuated by the same amount. You can disable this by specifying the flag "
        "--independently.");

    norm->add_option("target-decibels", m_target_decibels,
                     "The target level in decibels, where 0dB is the max volume.")
        ->required()
        ->check(CLI::Range(-200, 0));

    norm->add_flag(
        "--independently", m_normalise_independently,
        "When there are multiple files, normalise each one individually rather than by a common gain.");
    norm->add_flag("--rms", m_use_rms,
                   "Use RMS (root mean squared) calculations to work out the required gain amount.");
    return norm;
}

bool Normaliser::ProcessAudio(AudioFile &input, const std::string_view filename) {
    switch (m_current_stage) {
        case ProcessingStage::FindingCommonGain: {
            if (!ReadFileForCommonGain(input)) {
                m_successfully_found_common_gain = false;
            }
            return false;
        }
        case ProcessingStage::ApplyingGain: {
            return PerformNormalisation(input);
        }
        default: REQUIRE(0);
    }
    return false;
}

void Normaliser::Run(SubcommandHost &processor) {
    if (m_use_rms) {
        m_processor = std::make_unique<RMSGainCalculator>();
    } else {
        m_processor = std::make_unique<PeakGainCalculator>();
    }

    m_successfully_found_common_gain = true;
    if (processor.IsProcessingMultipleFiles() && !m_normalise_independently) {
        m_current_stage = ProcessingStage::FindingCommonGain;
        processor.ProcessAllFiles(*this);
    }

    if (!m_successfully_found_common_gain) {
        ErrorWithNewLine(
            "unable to perform normalisation because the common gain was not successfully found");
        return;
    }

    m_current_stage = ProcessingStage::ApplyingGain;
    processor.ProcessAllFiles(*this);
}

bool Normaliser::PerformNormalisation(AudioFile &input_audio) const {
    if (m_normalise_independently) {
        m_processor->RegisterBufferMagnitudes(input_audio);
    }
    const double gain = m_processor->GetGain(DBToAmp(m_target_decibels));

    MessageWithNewLine("Normaliser", "Applying a gain of ", gain);

    for (auto &s : input_audio.interleaved_samples) {
        s *= gain;
    }

    return true;
}

bool Normaliser::ReadFileForCommonGain(const AudioFile &audio) {
    return m_processor->RegisterBufferMagnitudes(audio);
}
