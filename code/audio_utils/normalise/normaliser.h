#pragma once
#include <cassert>
#include <memory>

#include "common.h"

class NormalisationGainCalculator {
  public:
    virtual ~NormalisationGainCalculator() {}
    virtual void AddBuffer(const AudioFile &audio) = 0;
    virtual float GetGain(float target_amp) const = 0;
};

class RMSGainCalculator : public NormalisationGainCalculator {
  public:
    void AddBuffer(const AudioFile &audio) override {
        if (!m_sum_of_squares_channels.size()) {
            m_sum_of_squares_channels.resize(audio.num_channels);
        }
        if (m_sum_of_squares_channels.size() != audio.num_channels) {
            FatalErrorWithNewLine("audio file has a different number of channels to a previous one - for RMS "
                                  "normalisation, all files must have the same number of channels");
        }
        for (size_t frame = 0; frame < audio.NumFrames(); ++frame) {
            for (auto channel = 0; channel < audio.num_channels; ++channel) {
                const auto sample_index = frame * audio.num_channels + channel;
                m_sum_of_squares_channels[channel] += std::pow(audio.interleaved_samples[sample_index], 2.0f);
            }
        }
        m_num_frames += audio.NumFrames();
    }

    float GetGain(float target_amp) const override {
        float smallest_gain = FLT_MAX;
        for (const auto sum_of_squares : m_sum_of_squares_channels) {
            const auto gain = GetLinearGainForTargetAmpRMS(target_amp, sum_of_squares, (float)m_num_frames);
            if (gain < smallest_gain) {
                smallest_gain = gain;
            }
        }
        return smallest_gain;
    }

  private:
    static float GetLinearGainForTargetAmpRMS(float target_amp, float sum_of_squares, float num_samples) {
        return std::sqrt((num_samples * std::pow(target_amp, 2.0f)) / sum_of_squares);
    }

    size_t m_num_frames {};
    std::vector<float> m_sum_of_squares_channels {};
};

class PeakGainCalculator : public NormalisationGainCalculator {
  public:
    void AddBuffer(const AudioFile &audio) override {
        float max_value = 0;
        for (const auto s : audio.interleaved_samples) {
            const auto magnitude = std::abs(s);
            if (magnitude > max_value) {
                max_value = magnitude;
            }
        }
        if (max_value > m_max_magnitude) {
            m_max_magnitude = max_value;
        }
    }

    float GetGain(float target_amp) const override { return target_amp / m_max_magnitude; }

  private:
    float m_max_magnitude;
};

class Normaliser : public Processor {
    void AddCLI(CLI::App &app) override;
    std::optional<AudioFile> Process(const AudioFile &input, ghc::filesystem::path &output_filename) override;
    void Run(AudioUtilInterface &audio_util) override;
    std::string GetDescription() override;

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
