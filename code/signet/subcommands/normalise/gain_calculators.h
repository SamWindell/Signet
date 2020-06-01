#pragma once

#include <algorithm>
#include <cfloat>

#include "audio_file.h"
#include "common.h"

class NormalisationGainCalculator {
  public:
    virtual ~NormalisationGainCalculator() {}
    virtual void RegisterBufferMagnitudes(const AudioFile &audio) = 0;
    virtual double GetGain(double target_amp) const = 0;
    virtual const char *GetName() const = 0;
    virtual double GetLargestRegisteredMagnitude() const = 0;
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

    double GetLargestRegisteredMagnitude() const override {
        double max_rms = 0;
        for (const auto sum_of_squares : m_sum_of_squares_channels) {
            const auto rms = std::sqrt((1.0f / (double)m_num_frames) * sum_of_squares);
            max_rms = std::max(max_rms, rms);
        }
        return max_rms;
    }

    double GetGain(double target_rms_amp) const override {
        double smallest_gain = DBL_MAX;
        for (const auto sum_of_squares : m_sum_of_squares_channels) {
            const auto gain =
                GetLinearGainForTargetAmpRMS(target_rms_amp, sum_of_squares, (double)m_num_frames);
            smallest_gain = std::min(smallest_gain, gain);
        }
        return smallest_gain;
    }

    const char *GetName() const override { return "RMS"; };

  private:
    static double
    GetLinearGainForTargetAmpRMS(double target_rms_amp, double sum_of_squares, double num_samples) {
        return std::sqrt((num_samples * std::pow(target_rms_amp, 2.0f)) / sum_of_squares);
    }

    size_t m_num_frames {};
    std::vector<double> m_sum_of_squares_channels {};
};

class PeakGainCalculator : public NormalisationGainCalculator {
  public:
    void RegisterBufferMagnitudes(const AudioFile &audio) override {
        double max_magnitude = 0;
        for (const auto s : audio.interleaved_samples) {
            const auto magnitude = std::abs(s);
            max_magnitude = std::max(max_magnitude, magnitude);
        }
        m_max_magnitude = std::max(m_max_magnitude, max_magnitude);
    }

    double GetLargestRegisteredMagnitude() const override { return m_max_magnitude; }

    double GetGain(double target_max_magnitude) const override {
        return target_max_magnitude / m_max_magnitude;
    }

    const char *GetName() const override { return "Peak"; };

  private:
    double m_max_magnitude;
};
