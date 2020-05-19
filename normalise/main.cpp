#include <assert.h>

#include "CLI11.hpp"
#include "common.h"

class Normaliser : public Processor {
    void AddCLI(CLI::App &app) override {
        app.add_option("-t,--target_decibels", m_target_decibels,
                       "The target level in decibels to convert the sample(s) to");
        app.add_flag("-c,--common_gain", m_common_gain,
                     "When using on a directory, amplifiy all the samples by the same amount");
        app.add_flag("--rms", m_use_rms, "Use RMS normalisation instead of peak");
    }

    std::optional<AudioFile> Process(const AudioFile &input,
                                     ghc::filesystem::path &output_filename) override {
        switch (m_current_mode) {
            case Mode::FindingCommonGain: {
                ReadFileForCommonGain(input);
                return {};
            }
            case Mode::ApplyingGain: {
                return PerformNormalisation(input);
            }
            default: assert(0);
        }
        return {};
    }

    void Run(AudioUtilInterface &audio_util) override {
        if (audio_util.IsProcessingMultipleFiles() && m_common_gain) {
            m_current_mode = Mode::FindingCommonGain;
            audio_util.ProcessAllFiles();
        }

        m_current_mode = Mode::ApplyingGain;
        audio_util.ProcessAllFiles();
    }

    std::string GetDescription() override { return "Normalise a sample to a certain level"; }

  private:
    static float GetMaxValueInBuffer(const AudioFile &buffer) {
        float max_value = 0;
        for (const auto s : buffer.interleaved_samples) {
            const auto magnitude = std::abs(s);
            if (magnitude > max_value) {
                max_value = magnitude;
            }
        }
        return max_value;
    }

    static float GetSumOfSampleSquares(const AudioFile &buffer) {
        float sum_of_squares = 0;
        for (const auto s : buffer.interleaved_samples) {
            sum_of_squares += std::pow(s, 2.0f);
        }
        return sum_of_squares;
    }

    static float GetLinearGainForTargetAmpRMS(float target_amp, float sum_of_squares, float num_samples) {
        return std::sqrt((num_samples * std::pow(target_amp, 2.0f)) / sum_of_squares);
    }

    AudioFile PerformNormalisation(const AudioFile &input_audio) const {
        const auto target_amp = DBToAmp(m_target_decibels);
        float gain = 0;
        if (m_num_accumulated) {
            if (m_use_rms) {
                gain = GetLinearGainForTargetAmpRMS(target_amp, m_sample_accumululator,
                                                    (float)m_num_accumulated);
            } else {
                const auto max_magitude = m_sample_accumululator;
                gain = target_amp / max_magitude;
            }
        } else {
            if (m_use_rms) {
                const auto n = (float)input_audio.interleaved_samples.size();
                gain = GetLinearGainForTargetAmpRMS(target_amp, GetSumOfSampleSquares(input_audio), n);
            } else {
                gain = target_amp / GetMaxValueInBuffer(input_audio);
            }
        }
        std::cout << "Applying a gain of " << gain << "\n";

        AudioFile result = input_audio;
        for (auto &s : result.interleaved_samples) {
            s *= gain;
        }

        return result;
    }

    void ReadFileForCommonGain(const AudioFile &audio) {
        if (m_use_rms) {
            m_sample_accumululator += GetSumOfSampleSquares(audio);
        } else {
            const auto max = GetMaxValueInBuffer(audio);
            if (max > m_sample_accumululator) {
                m_sample_accumululator = max;
            }
        }
        m_num_accumulated += audio.interleaved_samples.size();
    }

    enum class Mode {
        FindingCommonGain,
        ApplyingGain,
    };
    Mode m_current_mode {};
    float m_sample_accumululator = {};
    size_t m_num_accumulated = {};
    float m_gain = 0;

    bool m_common_gain = false;
    float m_target_decibels = 0.0f;
    bool m_use_rms = false;
};

int main(const int argc, const char *argv[]) {
    Normaliser normaliser {};
    AudioUtilInterface util(normaliser);
    return util.Main(argc, argv);
}
