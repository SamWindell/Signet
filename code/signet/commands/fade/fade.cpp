#include "fade.h"

#include "doctest.hpp"
#include "magic_enum.hpp"

#include "audio_file_io.h"
#include "common.h"
#include "test_helpers.h"

static tcb::span<const std::string_view> GetShapeNames() {
    static const auto names = magic_enum::enum_names<FadeCommand::Shape>();
    return {names.data(), names.size()};
}

CLI::App *FadeCommand::CreateCommandCLI(CLI::App &app) {
    auto fade = app.add_subcommand(
        "fade",
        "Adds a fade-in to the start and/or a fade-out to the end of the file(s). This subcommand has "
        "itself 2 subcommands, 'in' and 'out'; one of which must be specified. For each, you must specify "
        "first the fade length. You can then optionally specify the shape of the fade curve.");
    fade->require_subcommand();

    std::map<std::string, Shape> shape_name_dictionary;
    for (const auto &e : magic_enum::enum_entries<Shape>()) {
        shape_name_dictionary[std::string(e.second)] = e.first;
    }

    auto in = fade->add_subcommand("in", "Fade in the volume at the start of the file(s).");
    in->add_option("fade-in-length", m_fade_in_duration,
                   "The length of the fade in. " + AudioDuration::TypeDescription())
        ->required();

    in->add_option("fade-in-shape", m_fade_in_shape,
                   "The shape of the fade-in curve. The default is the 'sine' shape.")
        ->transform(CLI::CheckedTransformer(shape_name_dictionary, CLI::ignore_case));

    auto out = fade->add_subcommand("out", "Fade out the volume at the end of the file(s).");
    out->add_option("fade-out-length", m_fade_out_duration,
                    "The length of the fade out. " + AudioDuration::TypeDescription())
        ->required();

    out->add_option("fade-out-shape", m_fade_out_shape,
                    "The shape of the fade-out curve. The default is the 'sine' shape.")
        ->transform(CLI::CheckedTransformer(shape_name_dictionary, CLI::ignore_case));

    return fade;
}

static double GetFade(const FadeCommand::Shape shape, const s64 x_index, const s64 size) {
    REQUIRE(size);
    if (x_index == 0) return 0;
    if (x_index == size) return 1;

    const double x = (1.0 / (double)size) * x_index;
    static constexpr double silent_db = -30;
    static constexpr double range_db = -silent_db;
    switch (shape) {
        case FadeCommand::Shape::Linear: {
            return x;
        }
        case FadeCommand::Shape::Sine: {
            return std::sin(x * half_pi);
        }
        case FadeCommand::Shape::SCurve: {
            return (-(std::cos(x * pi) - 1.0)) / 2.0;
        }
        case FadeCommand::Shape::Exp: {
            return std::pow(0.5, (1 - x) * 5);
        }
        case FadeCommand::Shape::Log: {
            const auto db = AmpToDB(x);
            if (db < silent_db) {
                return x;
            }
            return (db + range_db) / range_db;
        }
        case FadeCommand::Shape::Sqrt: {
            return std::sqrt(x);
        }
        default: REQUIRE(0);
    }
    return 0;
}

void FadeCommand::PerformFade(AudioData &audio,
                              const s64 silent_frame,
                              const s64 fullvol_frame,
                              const FadeCommand::Shape shape) {
    const s64 increment = silent_frame < fullvol_frame ? 1 : -1;
    const auto size = std::abs(fullvol_frame - silent_frame);

    s64 pos = 0;
    for (s64 frame = silent_frame; frame != fullvol_frame; frame += increment) {
        const auto gain = GetFade(shape, pos++, size);
        for (unsigned channel = 0; channel < audio.num_channels; ++channel) {
            audio.GetSample(channel, frame) *= gain;
        }
    }
}

void FadeCommand::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        auto &audio = f.GetWritableAudio();
        if (m_fade_in_duration) {
            const auto fade_in_frames =
                std::min(audio.NumFrames() - 1,
                         m_fade_in_duration->GetDurationAsFrames(audio.sample_rate, audio.NumFrames()));
            PerformFade(audio, 0, (s64)fade_in_frames, m_fade_in_shape);

            MessageWithNewLine(GetName(), f, "Fading in {} frames with a {} curve", fade_in_frames,
                               magic_enum::enum_name(m_fade_in_shape));
        }
        if (m_fade_out_duration) {
            const auto fade_out_frames =
                m_fade_out_duration->GetDurationAsFrames(audio.sample_rate, audio.NumFrames());
            const auto last = (s64)audio.NumFrames() - 1;
            const auto start_frame = std::max<s64>(0, (s64)last - (s64)fade_out_frames);
            PerformFade(audio, last, start_frame, m_fade_out_shape);

            MessageWithNewLine(GetName(), f, "Fading out {} frames with a {} curve", fade_out_frames,
                               magic_enum::enum_name(m_fade_out_shape));
        }
    }
}

TEST_CASE("[FadeCommand]") {
    AudioData buf {};
    buf.sample_rate = 44100;
    buf.num_channels = 1;
    buf.interleaved_samples.resize(100, 1.0);
    buf.bits_per_sample = 16;

    SUBCASE("fade calculation") {
        const auto CheckRegion = [&](const s64 silent_frame, const s64 fullvol_frame) {
            for (const auto &shape : magic_enum::enum_values<FadeCommand::Shape>()) {
                CAPTURE(silent_frame);
                CAPTURE(fullvol_frame);
                FadeCommand::PerformFade(buf, silent_frame, fullvol_frame, shape);
                REQUIRE(buf.interleaved_samples[silent_frame] == 0.0);
                REQUIRE(buf.interleaved_samples[fullvol_frame] == 1.0);
                for (s64 i = std::min(silent_frame, fullvol_frame) + 1;
                     i < std::max(fullvol_frame, silent_frame) - 1; ++i) {
                    REQUIRE(buf.interleaved_samples[i] > 0);
                    REQUIRE(buf.interleaved_samples[i] < 1);
                }
            }
        };

        SUBCASE("forward partial") { CheckRegion(0, 10); }
        SUBCASE("backward partial") { CheckRegion(10, 0); }
        SUBCASE("forward whole") { CheckRegion(0, 99); }
        SUBCASE("backward whole") { CheckRegion(99, 0); }
    }

    SUBCASE("subcommand") {
        const std::string dir = "fader-test-files";
        if (!fs::is_directory(dir)) {
            fs::create_directory(dir);
        }

        const auto TestArgs = [&](std::string args, const size_t expected_fade_in_samples,
                                  const size_t expected_fade_out_samples) {
            CAPTURE(args);
            auto output = TestHelpers::ProcessBufferWithCommand<FadeCommand>(args, buf);
            REQUIRE(output);

            SUBCASE("audio fades in from 0 to 1") {
                if (Contains(args, " in ")) {
                    REQUIRE(buf.num_channels == 1);
                    REQUIRE(std::abs(output->interleaved_samples[0]) == 0.0);
                    for (size_t i = 0; i < expected_fade_in_samples; ++i) {
                        REQUIRE(output->interleaved_samples[i] < 1.0);
                        REQUIRE(output->interleaved_samples[i] >= 0);
                    }
                }
            }

            SUBCASE("audio fades out to 0") {
                if (Contains(args, " out ")) {
                    REQUIRE(buf.num_channels == 1);
                    REQUIRE(std::abs(output->interleaved_samples[output->interleaved_samples.size() - 1]) ==
                            0.0);
                    for (size_t i = output->interleaved_samples.size() - expected_fade_out_samples;
                         i < output->interleaved_samples.size(); ++i) {
                        REQUIRE(output->interleaved_samples[i] < 1.0);
                        REQUIRE(output->interleaved_samples[i] >= 0);
                    }
                }
            }

            for (const auto s : output->interleaved_samples) {
                REQUIRE(s >= 0);
                REQUIRE(s <= 1);
            }

            std::string output_name = dir + "/" + args;
            std::replace(output_name.begin(), output_name.end(), ' ', '_');
            output_name += ".wav";
            WriteAudioFile(output_name, *output);
        };

        const auto TestSuite = [&](auto shape) {
            const std::string shape_str {shape};
            TestArgs("fade out 10smp " + shape_str + " in 10smp " + shape_str, 10, 10);
            TestArgs("fade out 1smp " + shape_str + " in 1smp " + shape_str, 1, 1);
            TestArgs("fade out 60smp " + shape_str + " in 60smp " + shape_str, 60, 60);
            TestArgs("fade in 200smp " + shape_str, 100, 0);
            TestArgs("fade out 200smp " + shape_str, 0, 100);
        };

        for (const auto &n : GetShapeNames()) {
            TestSuite(n);
        }
    }
}
