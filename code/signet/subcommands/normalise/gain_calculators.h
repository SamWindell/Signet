#pragma once

#include <algorithm>
#include <float.h>

#include "audio_file.h"
#include "common.h"

class NormalisationGainCalculator {
  public:
    virtual ~NormalisationGainCalculator() {}
    virtual void RegisterBufferMagnitudes(const AudioFile &audio) = 0;
    virtual float GetGain(float target_amp) const = 0;
    virtual const char *GetName() const = 0;
    virtual float GetLargestRegisteredMagnitude() const = 0;
};

class RMSGainCalculator : public NormalisationGainCalculator {
  public:
    void RegisterBufferMagnitudes(const AudioFile &audio) override {
        if (!m_sum_of_squares_channels.size()) {
            m_sum_of_squares_channels.resize(audio.num_channels);
        }
        if (m_sum_of_squares_channels.size() != audio.num_channels) {
            FatalErrorWithNewLine("audio file has a different number of channels to a previous one - for RMS "
                                  "normalisation, all files must have the same number of channels");
        }
        for (size_t frame = 0; frame < audio.NumFrames(); ++frame) {
            for (unsigned channel = 0; channel < audio.num_channels; ++channel) {
                const auto sample_index = frame * audio.num_channels + channel;
                m_sum_of_squares_channels[channel] += std::pow(audio.interleaved_samples[sample_index], 2.0f);
            }
        }
        m_num_frames += audio.NumFrames();
    }

    float GetLargestRegisteredMagnitude() const override {
        float max_rms = 0;
        for (const auto sum_of_squares : m_sum_of_squares_channels) {
            const auto rms = std::sqrt((1.0f / (float)m_num_frames) * sum_of_squares);
            max_rms = std::max(max_rms, rms);
        }
        return max_rms;
    }

    float GetGain(float target_rms_amp) const override {
        float smallest_gain = FLT_MAX;
        for (const auto sum_of_squares : m_sum_of_squares_channels) {
            const auto gain =
                GetLinearGainForTargetAmpRMS(target_rms_amp, sum_of_squares, (float)m_num_frames);
            smallest_gain = std::min(smallest_gain, gain);
        }
        return smallest_gain;
    }

    const char *GetName() const override { return "RMS"; };

  private:
    static float GetLinearGainForTargetAmpRMS(float target_rms_amp, float sum_of_squares, float num_samples) {
        return std::sqrt((num_samples * std::pow(target_rms_amp, 2.0f)) / sum_of_squares);
    }

    size_t m_num_frames {};
    std::vector<float> m_sum_of_squares_channels {};
};

class PeakGainCalculator : public NormalisationGainCalculator {
  public:
    void RegisterBufferMagnitudes(const AudioFile &audio) override {
        float max_magnitude = 0;
        for (const auto s : audio.interleaved_samples) {
            const auto magnitude = std::abs(s);
            max_magnitude = std::max(max_magnitude, magnitude);
        }
        m_max_magnitude = std::max(m_max_magnitude, max_magnitude);
    }

    float GetLargestRegisteredMagnitude() const override { return m_max_magnitude; }

    float GetGain(float target_max_magnitude) const override {
        return target_max_magnitude / m_max_magnitude;
    }

    const char *GetName() const override { return "Peak"; };

  private:
    float m_max_magnitude;
};
