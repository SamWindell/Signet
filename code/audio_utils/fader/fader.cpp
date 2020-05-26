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
                                                                          {"scurve", Shape::SCurve},
                                                                          {"log", Shape::Log}},
                                            CLI::ignore_case));
}

static float GetFade(Fader::Shape shape, float x) {
    assert(x >= 0 && x <= 1);
    if (x == 0) return 0;
    if (x == 1) return 1;
    switch (shape) {
        case Fader::Shape::Linear: {
            return x;
        }
        case Fader::Shape::Sine: {
            return std::sin(x * half_pi);
        }
        case Fader::Shape::SCurve: {
            return (-(std::cos(x * pi) - 1.0f)) / 2.0f;
        }
        case Fader::Shape::Log: {
            constexpr float silent_db = -90;
            constexpr float range_db = -silent_db;
            return DBToAmp(silent_db + range_db * x);
        }
        default: assert(0);
    }
    return 0;
}

void PerformFade(AudioFile &audio,
                 const s64 silent_frame,
                 const s64 fullvol_frame,
                 const Fader::Shape shape) {
    const s64 increment = silent_frame < fullvol_frame ? 1 : -1;
    const float delta_x = 1.0f / ((float)std::abs(fullvol_frame - silent_frame) + 1);
    float x = 0;

    for (s64 frame = silent_frame; frame <= fullvol_frame; frame += increment) {
        const auto gain = GetFade(shape, x);
        for (unsigned channel = 0; channel < audio.num_channels; ++channel) {
            audio.GetSample(channel, frame) *= gain;
        }
        x += delta_x;
    }
}

std::optional<AudioFile> Fader::Process(const AudioFile &input, ghc::filesystem::path &output_filename) {
    AudioFile output = input;
    if (m_fade_out) {
        const auto fade_out_frames = m_fade_out->GetDurationAsFrames(output.sample_rate, output.NumFrames());
        const auto last = std::max<s64>(0, (s64)output.NumFrames() - 1);
        const auto start_frame = std::max<s64>(0, (s64)last - (s64)fade_out_frames);
        PerformFade(output, last, start_frame, m_shape);
    }
    if (m_fade_in) {
        const auto fade_in_frames = m_fade_in->GetDurationAsFrames(output.sample_rate, output.NumFrames());
        PerformFade(output, 0, (s64)fade_in_frames, m_shape);
    }
    return output;
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

        SUBCASE("audio fades in from 0 to 1") {
            if (has_in) {
                REQUIRE(buf.num_channels == 1);
                REQUIRE(std::abs(output->interleaved_samples[0]) == 0.0f);
                for (size_t i = 0; i < expected_fade_in_samples; ++i) {
                    REQUIRE(output->interleaved_samples[i] < 1.0f);
                }
            }
        }

        SUBCASE("audio fades out to 0") {
            if (has_out) {
                REQUIRE(buf.num_channels == 1);
                REQUIRE(std::abs(output->interleaved_samples[output->interleaved_samples.size() - 1]) ==
                        0.0f);
                for (size_t i = output->interleaved_samples.size() - expected_fade_out_samples;
                     i < output->interleaved_samples.size(); ++i) {
                    REQUIRE(output->interleaved_samples[i] < 1.0f);
                }
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
