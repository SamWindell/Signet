#pragma once
#include <array>
#include <tuple>

#include "audio_file.h"
#include "audio_util_interface.h"
#include "common.h"

class Duration {
    enum class Unit {
        Seconds,
        Milliseconds,
        Percent,
        Samples,
    };

  public:
    Duration() {}

    Duration(const std::string &string) {
        m_unit = *GetUnit(string);
        m_value = std::stof(string);
    }

    size_t GetFrames(unsigned sample_rate, size_t num_frames) {
        float result {};
        switch (m_unit) {
            case Unit::Seconds: result = sample_rate * m_value; break;
            case Unit::Milliseconds: result = sample_rate * (m_value / 1000.0f); break;
            case Unit::Percent: result = num_frames * (std::clamp(m_value, 0.0f, 100.0f) / 100.0f); break;
            case Unit::Samples: result = m_value; break;
            default: assert(0);
        }
        return std::max(num_frames, (size_t)result);
    }

    static std::optional<Unit> GetUnit(const std::string &str) {
        for (const auto u : available_units) {
            if (EndsWith(str, u.first)) {
                return u.second;
            }
        }
        return {};
    }

    static std::string ValidatorFunc(const std::string &str) {
        if (const auto unit = GetUnit(str); unit) {
            return {};
        } else {
            std::string error {"Value must be specified as one of the following units: "};
            for (const auto u : available_units) {
                error.append(u.first);
                error.append(" ");
            }
            return error;
        }
    }

  private:
    static constexpr std::pair<const char *, Unit> available_units[] = {{"s", Unit::Seconds},
                                                                        {"ms", Unit::Milliseconds},
                                                                        {"%", Unit::Percent},
                                                                        {"smp", Unit::Samples}};
    Unit m_unit {};
    float m_value {};
};

class Fader final : public Processor {
  public:
    void AddCLI(CLI::App &app) override {
        app.add_option("-o,--out", m_fade_out,
                       "Add a fade out at the end of the sample. You must specify the unit of this value.")
            ->check(Duration::ValidatorFunc, "Duration unit validator");
        app.add_option("-i,--in", m_fade_out,
                       "Add a fade in at the start of the sample. You must specify the unit of this value.")
            ->expected(2)
            ->check(Duration::ValidatorFunc, "Duration unit validator");
        app.add_option("-s,--shape", m_shape, "The shape of the curve")
            ->transform(CLI::CheckedTransformer(
                std::map<std::string, Shape> {{"linear", Shape::Linear}, {"curve", Shape::Curve}},
                CLI::ignore_case));
    }

    std::optional<AudioFile> Process(const AudioFile &input,
                                     ghc::filesystem::path &output_filename) override {
        AudioFile output = input;
        if (m_fade_out) {
            const auto fade_out_frames = m_fade_out->GetFrames(output.sample_rate, output.NumFrames());
            const auto start_frame = output.NumFrames() - fade_out_frames;
            PerformFade(output, start_frame, output.NumFrames(), m_shape, false);
        }
        if (m_fade_in) {
            const auto fade_in_frames = m_fade_in->GetFrames(output.sample_rate, output.NumFrames());
            PerformFade(output, 0, fade_in_frames, m_shape, true);
        }
        return output;
    }

    void Run(AudioUtilInterface &audio_util) override { audio_util.ProcessAllFiles(); }

    std::string GetDescription() override { return "Add a fade out to the samples"; }

  private:
    enum class Shape {
        Linear,
        Curve,
    };

    void PerformFade(AudioFile &audio, size_t begin, size_t end, Shape shape, bool fade_in) {
        for (size_t frame = begin; frame < end; ++frame) {
            const auto pos_through_fade = (float)(frame - begin) / (float)(end - begin);

            float multiplier;
            if (fade_in) {
                switch (shape) {
                    case Shape::Linear: multiplier = pos_through_fade;
                    case Shape::Curve: multiplier = std::sin(pos_through_fade * half_pi);
                    default: assert(0);
                }
            } else {
                switch (shape) {
                    case Shape::Linear: multiplier = 1.0f - pos_through_fade;
                    case Shape::Curve: multiplier = std::cos(pos_through_fade * half_pi);
                    default: assert(0);
                }
            }

            for (unsigned channel = 0; channel < audio.num_channels; ++channel) {
                audio.interleaved_samples[frame * audio.num_channels + channel] *= multiplier;
            }
        }
    }

    Shape m_shape = Shape::Linear;
    std::optional<Duration> m_fade_out {};
    std::optional<Duration> m_fade_in {};
};
