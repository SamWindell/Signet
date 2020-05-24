#include "normaliser.h"

#include "doctest.hpp"

void Normaliser::AddCLI(CLI::App &app) {
    app.add_option("-t,--target_decibels", m_target_decibels,
                   "The target level in decibels to convert the sample(s) to");
    app.add_flag("-c,--common_gain", m_use_common_gain,
                 "When using on a directory, amplifiy all the samples by the same amount");
    app.add_flag("--rms", m_use_rms, "Use RMS normalisation instead of peak");

    // add https://github.com/jiixyj/libebur128 too?
}

std::optional<AudioFile> Normaliser::Process(const AudioFile &input, ghc::filesystem::path &output_filename) {
    switch (m_current_stage) {
        case ProcessingStage::FindingCommonGain: {
            ReadFileForCommonGain(input);
            return {};
        }
        case ProcessingStage::ApplyingGain: {
            return PerformNormalisation(input);
        }
        default: assert(0);
    }
    return {};
}

void Normaliser::Run(AudioUtilInterface &audio_util) {
    if (m_use_rms) {
        m_processor = std::make_unique<RMSGainCalculator>();
    } else {
        m_processor = std::make_unique<PeakGainCalculator>();
    }

    if (audio_util.IsProcessingMultipleFiles() && m_use_common_gain) {
        m_current_stage = ProcessingStage::FindingCommonGain;
        audio_util.ProcessAllFiles();
    }

    m_current_stage = ProcessingStage::ApplyingGain;
    audio_util.ProcessAllFiles();
}

std::string Normaliser::GetDescription() { return "Normalise a sample to a certain level"; }

AudioFile Normaliser::PerformNormalisation(const AudioFile &input_audio) const {
    if (!m_use_common_gain) {
        m_processor->RegisterBufferMagnitudes(input_audio);
    }
    const float gain = m_processor->GetGain(DBToAmp(m_target_decibels));

    std::cout << "Applying a gain of " << gain << "\n";

    AudioFile result = input_audio;
    for (auto &s : result.interleaved_samples) {
        s *= gain;
    }

    return result;
}

void Normaliser::ReadFileForCommonGain(const AudioFile &audio) {
    m_processor->RegisterBufferMagnitudes(audio);
}
