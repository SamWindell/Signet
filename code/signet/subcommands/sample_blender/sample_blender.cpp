#include "sample_blender.h"

#include <memory>
#include <regex>

#include "filesystem.hpp"

#include "audio_file.h"
#include "backup.h"
#include "common.h"
#include "input_files.h"
#include "midi_pitches.h"
#include "pathname_expansion.h"
#include "string_utils.h"
#include "subcommands/tuner/tuner.h"
#include "test_helpers.h"

CLI::App *SampleBlender::CreateSubcommandCLI(CLI::App &app) {
    auto cli = app.add_subcommand(
        "sample-blender",
        R"aa(Multi-sample Sample Blender: creates samples in between other samples that are different pitches. It takes 2 samples and generates a set of samples in between them at a given semitone interval. Each generated sample is a different blend of the 2 base samples, tuned to match each other. This tool is useful when you have a multi-sampled instrument that was sampled only at large intervals; such as every octave. This tool can be used to create an instrument that sounds like it was sampled at smaller intervals.)aa");

    cli->add_option("root_note_regex", m_regex,
                    "Regex pattern containing 1 group that is to match the root note")
        ->required();

    cli->add_option("semitone-interval", m_semitone_interval,
                    "The semitone interval at which to generate new samples by")
        ->required();

    cli->add_option("out-filename", m_out_filename,
                    "The filename of the generated files (excluding extension). It should contain either the "
                    "substitution variable <root-num> or <root-note> which will be replaced by the root note "
                    "of the generated file. <root-num> is replaced by the MIDI note number, and <root-name> "
                    "is replaced by the note name, such as C3.")
        ->required()
        ->check([](const std::string &str) -> std::string {
            if (!Contains(str, "<root-num>") && !Contains(str, "<root-note>")) {
                return str + " does not at least one of either <root-num> or <root-note>";
            }
            return "";
        });

    return cli;
}

void SampleBlender::GenerateSamplesByBlending(SignetBackup &backup,
                                              const BaseBlendFiles &f1,
                                              const BaseBlendFiles &f2) {
    if (f1.root_note + m_semitone_interval >= f2.root_note) {
        MessageWithNewLine("SampleBlender", "Samples are close enough together already");
        return;
    }
    MessageWithNewLine("SampleBlender", "Blending between ", f1.path, " and ", f2.path);

    const auto max_semitone_distance = f2.root_note - f1.root_note;
    for (int root_note = f1.root_note + m_semitone_interval; root_note < f2.root_note;
         root_note += m_semitone_interval) {

        const auto distance_from_f1 =
            1 - ((double)(root_note - f1.root_note) / (double)max_semitone_distance);
        const auto distance_from_f2 =
            1 - ((double)(f2.root_note - root_note) / (double)max_semitone_distance);

        constexpr double cents_in_semitone = 100;

        AudioFile out = f1.file;
        const auto pitch_change1 = (root_note - f1.root_note) * cents_in_semitone;
        Tuner::ChangePitch(out, pitch_change1);
        out.MultiplyByScalar(distance_from_f1);

        AudioFile other = f2.file;
        const auto pitch_change2 = (root_note - f2.root_note) * cents_in_semitone;
        Tuner::ChangePitch(other, pitch_change2);
        other.MultiplyByScalar(distance_from_f2);

        out.AddOther(other);

        auto filename = m_out_filename;
        Replace(filename, "<root-num>", std::to_string(root_note));
        Replace(filename, "<root-note>", g_midi_pitches[root_note].name);

        const auto directory = f1.path.parent_path();
        const auto path = directory / (filename + "." + GetLowercaseExtension(f1.file.format));
        backup.CreateFile(path, out);
    }
}

void SampleBlender::GenerateFiles(const tcb::span<const InputAudioFile> input_files, SignetBackup &backup) {
    std::vector<BaseBlendFiles> files;
    for (const auto &p : input_files) {
        const std::regex r {m_regex};
        std::smatch pieces_match;
        const auto name = GetJustFilenameWithNoExtension(p.GetPath());

        if (std::regex_match(name, pieces_match, r)) {
            if (pieces_match.size() != 2) {
                ErrorWithNewLine("SampleBlender",
                                 "Expected exactly 1 regex group to be captured to represent the root note");
                return;
            }
            const auto root_note = std::stoi(pieces_match[1]);
            if (root_note < 0 || root_note > 127) {
                WarningWithNewLine("SampleBlender: root note of file ", name,
                                   " is not in the range 0-127 so cannot be processed");
            } else {
                files.push_back({p.GetPath(), root_note});
                MessageWithNewLine("SampleBlender", "found file ", files.back().path, " with root note ",
                                   files.back().root_note);
            }
        }
    }

    if (files.size() < 2) {
        ErrorWithNewLine("SampleBlender: regex pattern ", m_regex, " does not match any filenames");
        return;
    }

    REQUIRE(files.size() >= 2);

    std::sort(files.begin(), files.end(),
              [](const auto &a, const auto &b) { return a.root_note < b.root_note; });

    for (usize i = 0; i < files.size() - 1; ++i) {
        if (files[i].root_note == files[i + 1].root_note) {
            ErrorWithNewLine("2 files have the same root note, unable to perform blend: ", files[i].path,
                             " and ", files[i + 1].path);
            return;
        }
    }

    for (auto &f : files) {
        const auto file = ReadAudioFile(f.path);
        if (!file) {
            ErrorWithNewLine("SampleBlender could not open file");
            return;
        }
        f.file = std::move(*file);
    }

    for (usize i = 0; i < files.size() - 1; ++i) {
        GenerateSamplesByBlending(backup, files[i], files[i + 1]);
    }
}

TEST_CASE("SampleBlender") {
    if (!fs::is_directory("test-folder")) {
        fs::create_directory("test-folder");
    }
    {
        auto square = TestHelpers::CreateSquareWaveAtFrequency(1, 44100, 1, g_midi_pitches[57].pitch);
        square.MultiplyByScalar(0.25);
        REQUIRE(WriteAudioFile("test-folder/pitched-square-57.wav", square));

        auto sine = TestHelpers::CreateSineWaveAtFrequency(1, 44100, 1, g_midi_pitches[69].pitch);
        sine.MultiplyByScalar(0.25);
        REQUIRE(WriteAudioFile("test-folder/pitched-sine-69.wav", sine));

        auto square2 = TestHelpers::CreateSquareWaveAtFrequency(1, 44100, 1, g_midi_pitches[73].pitch);
        sine.MultiplyByScalar(0.25);
        REQUIRE(WriteAudioFile("test-folder/pitched-square-73.wav", square2));
    }

    // CLI::App app {"test"};
    // SignetBackup backup;

    // SampleBlender::Create(app, backup);

    // const auto args = TestHelpers::StringToArgs(
    //     "signet-gen sample-blender pitched-\\w*-(\\d+) test-folder 2 out-pitched-blend-<root-num>");
    // REQUIRE_NOTHROW(app.parse(args.Size(), args.Args()));
}
