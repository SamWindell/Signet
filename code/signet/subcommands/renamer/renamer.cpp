#include "renamer.h"

#include <regex>

#include "audio_file.h"
#include "common.h"
#include "midi_pitches.h"
#include "string_utils.h"
#include "subcommands/pitch_detector/pitch_detector.h"
#include "test_helpers.h"

CLI::App *Renamer::CreateSubcommandCLI(CLI::App &app) {
    auto renamer = app.add_subcommand("rename", "Change the filename of a file in various ways");
    renamer->require_subcommand();

    auto prefix =
        renamer->add_subcommand("prefix", "Add text to the end of the filename (before the extension)");
    prefix->add_option("prefix-text", m_prefix, "The string or variable to add to the end of the filename")
        ->required();

    auto suffix = renamer->add_subcommand("suffix", "Add text to the start of the filename");
    suffix->add_option("suffix-text", m_suffix, "The string or variable to add to the start of the filename")
        ->required();

    auto regex_replace = renamer->add_subcommand("regex-replace", "Replace the name using a reqex pattern");
    regex_replace->add_option("regex-pattern", m_regex_pattern, "The ECMAScript style regex pattern")
        ->required();
    regex_replace
        ->add_option("regex-replacement", m_regex_replacement,
                     "The replacement string, using brackets <> with the match number to specify where to "
                     "insert matches. For example <1>.")
        ->required();

    return renamer;
}

bool Renamer::ProcessFilename(const AudioFile &input,
                              std::string &filename,
                              const ghc::filesystem::path &full_path) {
    bool renamed = false;
    if (m_regex_pattern) {
        const std::regex r {*m_regex_pattern};
        std::smatch pieces_match;
        if (std::regex_match(filename, pieces_match, r)) {
            for (size_t i = 0; i < pieces_match.size(); ++i) {
                const std::ssub_match sub_match = pieces_match[i];
                const std::string piece = sub_match.str();
                Replace(m_regex_replacement, PutNumberInAngleBracket(i), piece);
            }
            filename = m_regex_replacement;
            renamed = true;
        }
    }

    if (m_prefix) {
        filename = *m_prefix + filename;
        renamed = true;
    }
    if (m_suffix) {
        filename = filename + *m_suffix;
        renamed = true;
    }

    if (renamed) {
        if (Contains(filename, "<counter>")) {
            Replace(filename, "<counter>", std::to_string(m_counter++));
        }
        if (Contains(filename, "<detected-pitch>") || Contains(filename, "<detected-midi-node>") ||
            Contains(filename, "<detected-note>")) {
            if (const auto pitch = PitchDetector::DetectPitch(input)) {
                const auto closest_musical_note = FindClosestMidiPitch(*pitch);

                Replace(filename, "<detected-pitch>", closest_musical_note.GetPitchString());
                Replace(filename, "<detected-midi-note>", std::to_string(closest_musical_note.midi_note));
                Replace(filename, "<detected-note>", closest_musical_note.name);
            } else {
                WarningWithNewLine(
                    "Renamer: One of the detected pitch variables was used in the file name, but we "
                    "could not find any pitch in the audio. All detected pitch variables will be replaced "
                    "with NOPITCH.");
                Replace(filename, "<detected-pitch>", "NOPITCH");
                Replace(filename, "<detected-midi-note>", "NOPITCH");
                Replace(filename, "<detected-note>", "NOPITCH");
            }
        }
        if (Contains(filename, "<parent-folder>")) {
            bool replaced = false;
            if (full_path.has_parent_path()) {
                const auto parent_folder = full_path.parent_path().filename();
                if (parent_folder != ".") {
                    Replace(filename, "<parent-folder>", parent_folder.generic_string());
                    replaced = true;
                }
            }
            if (!replaced) {
                WarningWithNewLine("Renamer: The file does not have a parent path, but the variable "
                                   "<parent-folder> was used. This will just be replaced by nothing.");
                Replace(filename, "<parent-folder>", "");
            }
        }
    }

    return renamed;
}

TEST_CASE("Renamer") {
    SUBCASE("requires args") {
        REQUIRE_THROWS(TestHelpers::ProcessFilenameWithSubcommand<Renamer>("rename", {}, "file.wav"));
        REQUIRE_THROWS(TestHelpers::ProcessFilenameWithSubcommand<Renamer>("rename prefix", {}, "file.wav"));
        REQUIRE_THROWS(TestHelpers::ProcessFilenameWithSubcommand<Renamer>("rename suffix", {}, "file.wav"));
        REQUIRE_THROWS(
            TestHelpers::ProcessFilenameWithSubcommand<Renamer>("rename regex-replace", {}, "file.wav"));
    }

    SUBCASE("prefix") {
        SUBCASE("simple") {
            const auto f =
                TestHelpers::ProcessFilenameWithSubcommand<Renamer>("rename prefix _", {}, "file.wav");
            REQUIRE(f);
            REQUIRE(*f == "_file");
        }
    }

    SUBCASE("suffix") {
        SUBCASE("simple") {
            const auto f =
                TestHelpers::ProcessFilenameWithSubcommand<Renamer>("rename suffix _", {}, "file.wav");
            REQUIRE(f);
            REQUIRE(*f == "file_");
        }
    }

    SUBCASE("regex-replace") {
        SUBCASE("replace everything") {
            const auto f = TestHelpers::ProcessFilenameWithSubcommand<Renamer>(
                "rename regex-replace .* new_name", {}, "file.wav");
            REQUIRE(f);
            REQUIRE(*f == "new_name");
        }
        SUBCASE("insert groups into replacement") {
            const auto f = TestHelpers::ProcessFilenameWithSubcommand<Renamer>(
                "rename regex-replace ([^l]*)(le) <1><2><1>", {}, "file.wav");
            REQUIRE(f);
            REQUIRE(*f == "filefi");
        }
    }

    SUBCASE("<parent-folder>") {
        SUBCASE("prefix") {
            const auto f = TestHelpers::ProcessFilenameWithSubcommand<Renamer>(
                "rename prefix <parent-folder>bar", {}, "foo/file.wav");
            REQUIRE(f);
            REQUIRE(*f == "foobarfile");
        }
        SUBCASE("suffix") {
            const auto f = TestHelpers::ProcessFilenameWithSubcommand<Renamer>(
                "rename suffix <parent-folder>bar", {}, "foo/file.wav");
            REQUIRE(f);
            REQUIRE(*f == "filefoobar");
        }
        SUBCASE("regex-replace") {
            const auto f = TestHelpers::ProcessFilenameWithSubcommand<Renamer>(
                "rename regex-replace .* <0><parent-folder>", {}, "foo/file.wav");
            REQUIRE(f);
            REQUIRE(*f == "filefoo");
        }
        SUBCASE("regex-replace") {
            const auto f = TestHelpers::ProcessFilenameWithSubcommand<Renamer>(
                "rename regex-replace .* <0><parent-folder>", {}, "foo/file.wav");
            REQUIRE(f);
            REQUIRE(*f == "filefoo");
        }
    }
}
