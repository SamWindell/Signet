#pragma once

#include <algorithm>
#include <cfloat>

#include "doctest.hpp"
#include "span.hpp"

#include "audio_data.h"
#include "common.h"

class NormalisationGainCalculator {
  public:
    virtual ~NormalisationGainCalculator() {}
    virtual bool RegisterBufferMagnitudes(const AudioData &audio, std::optional<unsigned> channel) = 0;
    virtual double GetGain(double target_amp) const = 0;
    virtual const char *GetName() const = 0;
    virtual double GetLargestRegisteredMagnitude() const = 0;
    virtual void Reset() {}
};

class RMSGainCalculator : public NormalisationGainCalculator {
  public:
    bool RegisterBufferMagnitudes(const AudioData &audio, std::optional<unsigned> channel) override {
        if (!m_sum_of_squares_channels.size()) {
            m_sum_of_squares_channels.resize(audio.num_channels);
        }
        if (m_sum_of_squares_channels.size() != audio.num_channels) {
            ErrorWithNewLine(
                "Norm", {},
                "Audio file has a different number of channels to a previous one - for RMS normalisation, all files must have the same number of channels");
            return false;
        }
        for (size_t frame = 0; frame < audio.NumFrames(); ++frame) {
            for (unsigned chan = 0; chan < audio.num_channels; ++chan) {
                const auto sample_index = frame * audio.num_channels + chan;
                m_sum_of_squares_channels[chan] += std::pow(audio.interleaved_samples[sample_index], 2.0);
            }
        }
        m_num_frames += audio.NumFrames();
        return true;
    }

    double GetLargestRegisteredMagnitude() const override {
        if (!m_num_frames) return 0;
        double max_rms = 0;
        for (const auto sum_of_squares : m_sum_of_squares_channels) {
            const auto rms = std::sqrt((1.0 / (double)m_num_frames) * sum_of_squares);
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

    void Reset() override {
        m_sum_of_squares_channels.clear();
        m_num_frames = 0;
    }

  private:
    static double
    GetLinearGainForTargetAmpRMS(double target_rms_amp, double sum_of_squares, double num_samples) {
        return std::sqrt((num_samples * std::pow(target_rms_amp, 2.0)) / sum_of_squares);
    }

    size_t m_num_frames {};
    std::vector<double> m_sum_of_squares_channels {};
};

class PeakGainCalculator : public NormalisationGainCalculator {
  public:
    bool RegisterBufferMagnitudes(const AudioData &audio, std::optional<unsigned> channel) override {
        double max_magnitude = 0;

        for (size_t frame = 0; frame < audio.NumFrames(); ++frame) {
            if (channel) {
                max_magnitude = std::max(max_magnitude, std::abs(audio.GetSample(*channel, frame)));
            } else {
                for (unsigned chan = 0; chan < audio.num_channels; ++chan) {
                    max_magnitude = std::max(max_magnitude, std::abs(audio.GetSample(chan, frame)));
                }
            }
        }
        m_max_magnitude = std::max(m_max_magnitude, max_magnitude);
        REQUIRE(m_max_magnitude >= 0);
        return true;
    }

    double GetLargestRegisteredMagnitude() const override { return m_max_magnitude; }

    double GetGain(double target_max_magnitude) const override {
        if (!m_max_magnitude) return 0;
        return target_max_magnitude / m_max_magnitude;
    }

    const char *GetName() const override { return "Peak"; };

    void Reset() override { m_max_magnitude = 0; }

  private:
    double m_max_magnitude {};
};

void NormaliseToTarget(AudioData &audio, const double target_amp);
void NormaliseToTarget(std::vector<double> &samples, const double target_amp);
double GetRMS(const tcb::span<const double> samples);
