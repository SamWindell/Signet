#include "normaliser.h"

#include "CLI11.hpp"
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

void Normaliser::ProcessFiles(const tcb::span<EditTrackedAudioFile> files) {
    if (m_use_rms) {
        m_processor = std::make_unique<RMSGainCalculator>();
    } else {
        m_processor = std::make_unique<PeakGainCalculator>();
    }

    m_successfully_found_common_gain = true;
    if (files.size() < 1 && !m_normalise_independently) {
        for (auto &f : files) {
            if (!ReadFileForCommonGain(f.GetAudio())) {
                m_successfully_found_common_gain = false;
            }
        }
    }

    if (!m_successfully_found_common_gain) {
        ErrorWithNewLine(
            "unable to perform normalisation because the common gain was not successfully found");
        return;
    }

    for (auto &f : files) {
        PerformNormalisation(f.GetWritableAudio());
    }
}

bool Normaliser::PerformNormalisation(AudioData &input_audio) const {
    if (m_normalise_independently) {
        m_processor->Reset();
        m_processor->RegisterBufferMagnitudes(input_audio);
    }
    const double gain = m_processor->GetGain(DBToAmp(m_target_decibels));

    MessageWithNewLine("Normaliser", "Applying a gain of ", gain);

    for (auto &s : input_audio.interleaved_samples) {
        s *= gain;
    }

    return true;
}

bool Normaliser::ReadFileForCommonGain(const AudioData &audio) {
    return m_processor->RegisterBufferMagnitudes(audio);
}
