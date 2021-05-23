#include "sample_blend.h"

#include <memory>
#include <regex>

#include "filesystem.hpp"

#include "audio_file_io.h"
#include "backup.h"
#include "common.h"
#include "filepath_set.h"
#include "midi_pitches.h"
#include "string_utils.h"
#include "test_helpers.h"

CLI::App *SampleBlendCommand::CreateCommandCLI(CLI::App &app) {
    auto cli = app.add_subcommand(
        "sample-blend",
        R"aa(Creates samples in between other samples that are different pitches. It takes 2 samples and generates a set of samples in between them at a given semitone interval. Each generated sample is a different blend of the 2 base samples, tuned to match each other. This tool is useful when you have a multi-sampled instrument that was sampled only at large intervals; such as every octave. This tool can be used to create an instrument that sounds like it was sampled at smaller intervals.)aa");

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

    cli->add_flag(
        "--make-same-length", m_make_same_length,
        "For each generated file, if the 2 files that are being combined are not the same length, the longer one will be trimmed to the same length as the shorter before they are blended.");

    return cli;
}

void SampleBlendCommand::GenerateSamplesByBlending(SignetBackup &backup,
                                                   BaseBlendFile &f1,
                                                   BaseBlendFile &f2) {
    if (f1.root_note + m_semitone_interval >= f2.root_note) {
        MessageWithNewLine(GetName(), "Samples are close enough together already");
        return;
    }
    MessageWithNewLine(GetName(), "Blending between {} and {}", f1.file->GetPath(), f2.file->GetPath());

    const auto max_semitone_distance = f2.root_note - f1.root_note;
    for (int root_note = f1.root_note + m_semitone_interval; root_note < f2.root_note;
         root_note += m_semitone_interval) {

        const auto distance_from_f1 =
            1 - ((double)(root_note - f1.root_note) / (double)max_semitone_distance);
        const auto distance_from_f2 =
            1 - ((double)(f2.root_note - root_note) / (double)max_semitone_distance);

        constexpr double cents_in_semitone = 100;

        AudioData out = f1.file->GetAudio();
        const auto pitch_change1 = (root_note - f1.root_note) * cents_in_semitone;
        out.ChangePitch(pitch_change1);
        out.MultiplyByScalar(distance_from_f1);

        AudioData other = f2.file->GetAudio();
        const auto pitch_change2 = (root_note - f2.root_note) * cents_in_semitone;
        other.ChangePitch(pitch_change2);
        other.MultiplyByScalar(distance_from_f2);

        if (m_make_same_length) {
            out.interleaved_samples.resize(
                std::min(out.interleaved_samples.size(), other.interleaved_samples.size()));
        }

        out.AddOther(other);

        auto filename = m_out_filename;
        Replace(filename, "<root-num>", std::to_string(root_note));
        Replace(filename, "<root-note>", g_midi_pitches[root_note].name);

        const auto directory = f1.file->GetPath().parent_path();
        const auto path = directory / (filename + "." + GetLowercaseExtension(f1.file->GetAudio().format));
        backup.CreateFile(path, out, false);
    }
}

void SampleBlendCommand::GenerateFiles(AudioFiles &input_files, SignetBackup &backup) {
    std::map<fs::path, std::vector<BaseBlendFile>> base_file_folders;
    for (auto [folder, files] : input_files.Folders()) {
        for (auto &f : files) {
            const std::regex r {m_regex};
            std::smatch pieces_match;
            const auto name = GetJustFilenameWithNoExtension(f->GetPath());

            if (std::regex_match(name, pieces_match, r)) {
                if (pieces_match.size() != 2) {
                    ErrorWithNewLine(
                        GetName(),
                        "Expected exactly 1 regex group to be captured to represent the root note");
                    return;
                }
                const auto root_note = std::stoi(pieces_match[1]);
                if (root_note < 0 || root_note > 127) {
                    WarningWithNewLine(
                        GetName(), "Root note of file {} is not in the range 0-127 so cannot be processed",
                        name);
                } else {
                    auto &vec = base_file_folders[folder];
                    vec.emplace_back(f, root_note);
                    MessageWithNewLine(GetName(), "Found file {} with root note {}", f->GetPath(), root_note);
                }
            }
        }
    }

    for (auto &[folder, files] : base_file_folders) {
        if (files.size() < 2) {
            WarningWithNewLine(GetName(), "regex pattern {} does not match at least 2 filenames in folder {}",
                               m_regex, folder);
            continue;
        }
        std::sort(files.begin(), files.end(),
                  [](const auto &a, const auto &b) { return a.root_note < b.root_note; });

        for (usize i = 0; i < files.size() - 1; ++i) {
            if (files[i].root_note == files[i + 1].root_note) {
                WarningWithNewLine(
                    GetName(),
                    "2 files have the same root note, unable to perform blend: {} and {} in folder {}",
                    files[i].file->GetPath(), files[i + 1].file->GetPath(), folder);
                continue;
            }
        }

        for (usize i = 0; i < files.size() - 1; ++i) {
            GenerateSamplesByBlending(backup, files[i], files[i + 1]);
        }
    }
}

TEST_CASE("SampleBlendCommand") {
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

    // SampleBlendCommand::Create(app, backup);

    // const auto args = TestHelpers::StringToArgs(
    //     "signet-gen sample-blender pitched-\\w*-(\\d+) test-folder 2 out-pitched-blend-<root-num>");
    // REQUIRE_NOTHROW(app.parse(args.Size(), args.Args()));
}
