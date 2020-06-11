#include "renamer.h"

#include <regex>

#include "audio_file.h"
#include "common.h"
#include "midi_pitches.h"
#include "string_utils.h"
#include "subcommands/pitch_detector/pitch_detector.h"
#include "test_helpers.h"

static const std::string replacement_variables_info = R"foo(
<counter>             A unique number starting from zero. The ordering of these numbers is not specified.
<detected-pitch>      The detected pitch of audio file in Hz. If no pitch is found this variable will be empty.
<detected-midi-note>  The MIDI note number that is closest to the detected pitch of the audio file. If no pitch is found this variable will be empty.
<detected-note>       The musical note-name that is closest to the detected pitch of the audio file. The note is capitalised, and the octave number is specified. For example 'C3'. If no pitch is found this variable will be empty.
<parent-folder>       The name of the folder that contains the audio file.)foo";

CLI::App *Renamer::CreateSubcommandCLI(CLI::App &app) {
    auto renamer = app.add_subcommand(
        "rename",
        "Renamer: Renames the file. All options for this subcommand relate to just the name of the file - "
        "not the folder or the file extension. Text added via this command can contain special substitution "
        "variables; these will be replaced by values specified in this list: " +
            replacement_variables_info);
    renamer->require_subcommand();

    auto prefix = renamer->add_subcommand("prefix", "Add text to the start of the filename.");
    prefix->add_option("prefix-text", m_prefix, "The text to add, may contain substitution variables.")
        ->required();

    auto suffix =
        renamer->add_subcommand("suffix", "Add text to the end of the filename (before the extension).");
    suffix->add_option("suffix-text", m_suffix, "The text to add, may contain substitution variables.")
        ->required();

    auto regex_replace =
        renamer->add_subcommand("regex-replace", "Replace names that match the given regex pattern.");
    regex_replace
        ->add_option("regex-pattern", m_regex_pattern,
                     "The ECMAScript-style regex pattern to match filenames against - folder names or file "
                     "extensions are ignored.")
        ->required();
    regex_replace
        ->add_option("regex-replacement", m_regex_replacement,
                     "The new filename for files that matched the regex. This may contain substitution "
                     "variables. Matching groups from the regex can also be substituted into this new name. "
                     "You achieve this similarly to the special variable substitution. However, this time "
                     "you are put the regex group index in the angle-brackets (such as <1>). Remember that "
                     "with regex, group index 0 is always the whole match.")
        ->required();

    return renamer;
}

bool Renamer::ProcessFilename(fs::path &path, const AudioFile &input) {
    std::string filename = GetJustFilenameWithNoExtension(path);

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
        if (Contains(filename, "<detected-pitch>") || Contains(filename, "<detected-midi-note>") ||
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
                    "substituted with nothing.");
                Replace(filename, "<detected-pitch>", "");
                Replace(filename, "<detected-midi-note>", "");
                Replace(filename, "<detected-note>", "");
            }
        }
        if (Contains(filename, "<parent-folder>")) {
            bool replaced = false;
            if (path.has_parent_path()) {
                const auto parent_folder = path.parent_path().filename();
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

    if (renamed) {
        const auto ext = path.extension();
        path.replace_filename(filename);
        path.replace_extension(ext);
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
