#include "fader.h"

#include "doctest.hpp"
#include "magic_enum.hpp"

#include "audio_file.h"
#include "common.h"

tcb::span<const std::string_view> Fader::GetShapeNames() {
    static const auto names = magic_enum::enum_names<Fader::Shape>();
    return {names.data(), names.size()};
}

void Fader::AddCLI(CLI::App &app) {
    app.add_option("-o,--out", m_fade_out,
                   "Add a fade out at the end of the sample. You must specify the unit of this value.")
        ->check(AudioDuration::ValidateString, AudioDuration::ValidatorDescription());
    app.add_option("-i,--in", m_fade_in,
                   "Add a fade in at the start of the sample. You must specify the unit of this value.")
        ->check(AudioDuration::ValidateString, AudioDuration::ValidatorDescription());
    app.add_option("-s,--shape", m_shape, "The shape of the curve")
        ->transform(CLI::CheckedTransformer(std::map<std::string, Shape> {{"linear", Shape::Linear},
                                                                          {"sine", Shape::Sine},
                                                                          {"scurve", Shape::SCurve}},
                                            CLI::ignore_case));
}

std::optional<AudioFile> Fader::Process(const AudioFile &input, ghc::filesystem::path &output_filename) {
    AudioFile output = input;
    if (m_fade_out) {
        const auto fade_out_frames = m_fade_out->GetDurationAsFrames(output.sample_rate, output.NumFrames());
        const auto last = (size_t)std::max(0, (int)output.NumFrames() - 1);
        const auto start_frame = (size_t)std::max(0, (int)last - (int)fade_out_frames);
        PerformFade(output, start_frame, last, m_shape, false);
    }
    if (m_fade_in) {
        const auto fade_in_frames = m_fade_in->GetDurationAsFrames(output.sample_rate, output.NumFrames());
        PerformFade(output, 0, fade_in_frames, m_shape, true);
    }
    return output;
}

void Fader::PerformFade(AudioFile &audio,
                        const size_t first,
                        const size_t last,
                        const Shape shape,
                        const bool fade_in) {
    for (size_t frame = first; frame <= last; ++frame) {
        const auto x = (float)(frame - first) / (float)(last - first);
        assert(x >= 0 && x <= 1);

        float multiplier = -1;
        switch (shape) {
            case Shape::Linear: {
                multiplier = fade_in ? x : 1.0f - x;
                break;
            }
            case Shape::Sine: {
                const auto angle = x * half_pi;
                multiplier = fade_in ? std::sin(angle) : std::cos(angle);
                break;
            }
            case Shape::SCurve: {
                const auto y = (-(std::cos(x * pi) - 1.0f)) / 2.0f;
                multiplier = fade_in ? y : (1.0f - y);
                break;
            }
            default: assert(0);
        }
        assert(multiplier != -1);

        for (unsigned channel = 0; channel < audio.num_channels; ++channel) {
            // ensure the start and end are 0
            if (fade_in && frame == 0) multiplier = 0;
            if (!fade_in && frame == last) multiplier = 0;

            audio.GetSample(channel, frame) *= multiplier;
        }
    }
}

TEST_CASE("[Fader] args") {
    AudioFile buf {};
    buf.sample_rate = 100;
    buf.num_channels = 1;
    buf.interleaved_samples.resize(100, 1.0f);

    const auto TestArgs = [&](const std::initializer_list<const char *> args,
                              const size_t expected_fade_in_samples, const size_t expected_fade_out_samples) {
        Fader fader {};
        CLI::App app;
        fader.AddCLI(app);
        REQUIRE_NOTHROW(app.parse((int)args.size(), args.begin()));
        const bool has_out = app.count("--out") == 1;
        const bool has_in = app.count("--in") == 1;

        ghc::filesystem::path filename {};
        const auto output = fader.Process(buf, filename);
        REQUIRE(output);

        if (has_in) {
            assert(buf.num_channels == 1);
            REQUIRE(std::abs(output->interleaved_samples[0]) == 0.0f);
            for (size_t i = 0; i < expected_fade_in_samples; ++i) {
                REQUIRE(output->interleaved_samples[i] < 1.0f);
            }
        }

        if (has_out) {
            assert(buf.num_channels == 1);
            REQUIRE(std::abs(output->interleaved_samples[output->interleaved_samples.size() - 1]) == 0.0f);
            for (size_t i = output->interleaved_samples.size() - expected_fade_out_samples;
                 i < output->interleaved_samples.size(); ++i) {
                REQUIRE(output->interleaved_samples[i] < 1.0f);
            }
        }
    };

    const auto TestSuite = [&](auto shape) {
        const std::string shape_str {shape};
        CAPTURE(shape_str);
        TestArgs({"test.exe", "--out", "10smp", "--in", "10smp", "--shape", shape_str.data()}, 10, 10);
        TestArgs({"test.exe", "--out", "1smp", "--in", "1smp", "--shape", shape_str.data()}, 1, 1);
        TestArgs({"test.exe", "--out", "60smp", "--in", "60smp", "--shape", shape_str.data()}, 60, 60);
        TestArgs({"test.exe", "--in", "200smp", "--shape", shape_str.data()}, 100, 0);
        TestArgs({"test.exe", "--out", "200smp", "--shape", shape_str.data()}, 0, 100);
    };

    for (const auto n : Fader::GetShapeNames()) {
        TestSuite(n);
    }
}
