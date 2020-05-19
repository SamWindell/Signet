#include <assert.h>

#include "CLI11.hpp"
#include "common.h"

class Normaliser : public Processor {
    void AddCLI(CLI::App &app) override {
        app.add_option("-t,--target_decibels", m_target_decibels,
                       "The target level in decibels to convert the sample(s) to");
        app.add_flag("-c,--common_gain", m_common_gain,
                     "When using on a directory, amplifiy all the samples by the same amount");
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

    AudioFile PerformNormalisation(const AudioFile &input_audio) const {
        float max_magitude = m_max_sample_magnitude;
        if (max_magitude == 0) {
            max_magitude = GetMaxValueInBuffer(input_audio);
        }

        const auto target_amp = DBToAmp(m_target_decibels);
        const auto gain = target_amp / max_magitude;
        std::cout << "Max value is " << max_magitude << ", applying a gain of " << gain << "\n";

        AudioFile result = input_audio;
        for (auto &s : result.interleaved_samples) {
            s *= gain;
        }

        return result;
    }

    void ReadFileForCommonGain(const AudioFile &audio) {
        const auto max = GetMaxValueInBuffer(audio);
        if (max > m_max_sample_magnitude) {
            m_max_sample_magnitude = max;
        }
    }

    enum class Mode {
        FindingCommonGain,
        ApplyingGain,
    };
    Mode m_current_mode {};
    float m_max_sample_magnitude = 0;
    float m_gain = 0;

    bool m_common_gain = false;
    float m_target_decibels = 0.0f;
};

int main(const int argc, const char *argv[]) {
    Normaliser normaliser {};
    AudioUtilInterface util(normaliser);
    return util.Main(argc, argv);
}
