#include "fader.h"

#include "doctest.hpp"
#include "magic_enum.hpp"

#include "audio_file.h"
#include "common.h"

tcb::span<const std::string_view> Fader::GetShapeNames() {
    static const auto names = magic_enum::enum_names<Fader::Shape>();
    return {names.data(), names.size()};
}

CLI::App *Fader::CreateSubcommandCLI(CLI::App &app) {
    auto fade = app.add_subcommand("fade", "Add a fade to the start or and of the audio.");
    fade->require_subcommand();

    std::map<std::string, Shape> shape_name_dictionary;
    for (const auto e : magic_enum::enum_entries<Shape>()) {
        shape_name_dictionary[std::string(e.second)] = e.first;
    }

    auto in = fade->add_subcommand("in", "Fade in the volume at the start of the audio.");
    in->add_option("length", m_fade_in_duration,
                   "The length of the fade in. You must specify the unit for this value.")
        ->required();
    in->add_option("shape", m_fade_in_shape, "The shape of the fade-in curve")
        ->transform(CLI::CheckedTransformer(shape_name_dictionary, CLI::ignore_case));

    auto out = fade->add_subcommand("out", "Fade out the volume at the end of the audio.");
    out->add_option("length", m_fade_out_duration,
                    "The length of the fade out. You must specify the unit for this value.")
        ->required();
    out->add_option("shape", m_fade_out_shape, "The shape of the fade-out curve")
        ->transform(CLI::CheckedTransformer(shape_name_dictionary, CLI::ignore_case));

    return fade;
}

static float GetFade(Fader::Shape shape, float x) {
    assert(x >= 0 && x <= 1);
    if (x == 0) return 0;
    if (x == 1) return 1;
    static constexpr float silent_db = -90;
    static constexpr float range_db = -silent_db;
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
        case Fader::Shape::Exp: {
            // TODO: this and exp are the same thing??
            return DBToAmp(silent_db + range_db * x);
        }
        case Fader::Shape::Log: {
            // TODO: this and log are the same thing??
            return std::clamp((AmpToDB(x) + range_db) / range_db, 0.0f, 1.0f);
        }
        case Fader::Shape::Sqrt: {
            return std::sqrt(x);
        }
        default: assert(0);
    }
    return 0;
}

static void
PerformFade(AudioFile &audio, const s64 silent_frame, const s64 fullvol_frame, const Fader::Shape shape) {
    const s64 increment = silent_frame < fullvol_frame ? 1 : -1;
    const float delta_x = 1.0f / ((float)std::abs(fullvol_frame - silent_frame) + 1);
    float x = 0;

    for (s64 frame = silent_frame; frame != fullvol_frame + increment; frame += increment) {
        const auto gain = GetFade(shape, x);
        for (unsigned channel = 0; channel < audio.num_channels; ++channel) {
            audio.GetSample(channel, frame) *= gain;
        }
        x += delta_x;
    }
}

std::optional<AudioFile> Fader::Process(const AudioFile &input, ghc::filesystem::path &output_filename) {
    AudioFile output = input;
    if (m_fade_in_duration) {
        const auto fade_in_frames =
            std::min(output.NumFrames() - 1,
                     m_fade_in_duration->GetDurationAsFrames(output.sample_rate, output.NumFrames()));
        PerformFade(output, 0, (s64)fade_in_frames, m_fade_in_shape);
    }
    if (m_fade_out_duration) {
        const auto fade_out_frames =
            m_fade_out_duration->GetDurationAsFrames(output.sample_rate, output.NumFrames());
        const auto last = std::max<s64>(0, (s64)output.NumFrames() - 1);
        const auto start_frame = std::max<s64>(0, (s64)last - (s64)fade_out_frames);
        PerformFade(output, last, start_frame, m_fade_out_shape);
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
        fader.CreateSubcommandCLI(app);
        REQUIRE_NOTHROW(app.parse((int)args.size(), args.begin()));

        ghc::filesystem::path filename {};
        const auto output = fader.Process(buf, filename);
        REQUIRE(output);

        SUBCASE("audio fades in from 0 to 1") {
            if (fader.HasFadeIn()) {
                REQUIRE(buf.num_channels == 1);
                REQUIRE(std::abs(output->interleaved_samples[0]) == 0.0f);
                for (size_t i = 0; i < expected_fade_in_samples; ++i) {
                    REQUIRE(output->interleaved_samples[i] < 1.0f);
                }
            }
        }

        SUBCASE("audio fades out to 0") {
            if (fader.HasFadeOut()) {
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
        TestArgs({"signet", "fade", "out", "10smp", shape_str.data(), "in", "10smp", shape_str.data()}, 10,
                 10);
        TestArgs({"signet", "fade", "out", "1smp", shape_str.data(), "in", "1smp", shape_str.data()}, 1, 1);
        TestArgs({"signet", "fade", "out", "60smp", shape_str.data(), "in", "60smp", shape_str.data()}, 60,
                 60);
        TestArgs({"signet", "fade", "in", "200smp", shape_str.data()}, 100, 0);
        TestArgs({"signet", "fade", "out", "200smp", shape_str.data()}, 0, 100);
    };

    for (const auto n : Fader::GetShapeNames()) {
        TestSuite(n);
    }
}
