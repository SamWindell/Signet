#include "sample_blender.h"

#include <memory>
#include <regex>

#include "filesystem.hpp"

#include "audio_file.h"
#include "common.h"
#include "midi_pitches.h"
#include "pathname_expansion.h"
#include "string_utils.h"
#include "subcommands/tuner/tuner.h"
#include "test_helpers.h"

void SampleBlender::Create(CLI::App &app) {
    auto cli = app.add_subcommand(
        "sample-blender",
        "Sample Blender: Generates new samples by cross-fading between samples in a given directory.");

    auto sample_blender = std::make_shared<SampleBlender>();
    cli->add_option("root_note_regex", sample_blender->m_regex,
                    "Regex pattern containing 1 group that is to match the root note")
        ->required();

    cli->add_option("directory", sample_blender->m_directory, "Directory to search for files in")->required();

    cli->add_option("semitone-interval", sample_blender->m_semitone_interval,
                    "The semitone interval at which to generate new samples by")
        ->required();

    cli->callback([sample_blender]() { sample_blender->Run(); });
}

struct ProcessFiles {
    fs::path path;
    int root_note;
    AudioFile file;
};

static void GenerateSamplesByBlending(const ProcessFiles &f1,
                                      const ProcessFiles &f2,
                                      const int semitone_interval,
                                      const fs::path &dir) {
    if (f1.root_note + semitone_interval >= f2.root_note) {
        MessageWithNewLine("SampleBlender", "Samples are close enough together already");
        return;
    }
    MessageWithNewLine("SampleBlender", "Blending between ", f1.path, " and ", f2.path);

    const auto max_semitone_distance = f2.root_note - f1.root_note;
    for (int root_note = f1.root_note + semitone_interval; root_note < f2.root_note;
         root_note += semitone_interval) {

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

        const auto filename = dir.generic_string() + "/blended-sample-" + std::to_string(root_note) + "." +
                              GetLowercaseExtension(f1.file.format);
        WriteAudioFile(filename, out);
    }
}

void SampleBlender::Run() {
    std::vector<fs::path> paths;
    ForEachFileInDirectory(m_directory.generic_string(), false, [&](const auto &p) {
        if (IsAudioFileReadable(p)) paths.push_back({p});
    });

    std::vector<ProcessFiles> files;
    for (auto &p : paths) {
        const std::regex r {m_regex};
        std::smatch pieces_match;
        const auto name = GetJustFilenameWithNoExtension(p);

        if (std::regex_match(name, pieces_match, r)) {
            if (pieces_match.size() != 2) {
                ErrorWithNewLine("SampleBlender",
                                 "Expected exactly 1 regex group to be captured to represent the root note");
                return;
            }
            files.push_back({p, std::stoi(pieces_match[1])});
            MessageWithNewLine("SampleBlender", "found file ", files.back().path, " with root note ",
                               files.back().root_note);
        }
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
        GenerateSamplesByBlending(files[i], files[i + 1], m_semitone_interval, m_directory);
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

        auto sqare2 = TestHelpers::CreateSquareWaveAtFrequency(1, 44100, 1, g_midi_pitches[73].pitch);
        sine.MultiplyByScalar(0.25);
        REQUIRE(WriteAudioFile("test-folder/pitched-square-73.wav", sine));
    }

    CLI::App app {"test"};

    SampleBlender::Create(app);

    const auto args =
        TestHelpers::StringToArgs("signet-gen sample-blender pitched-\\w*-(\\d+) test-folder 1");
    REQUIRE_NOTHROW(app.parse(args.Size(), args.Args()));
}
