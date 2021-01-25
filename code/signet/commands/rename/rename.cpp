#include "rename.h"

#include <regex>

#include "audio_file_io.h"
#include "common.h"
#include "midi_pitches.h"
#include "rename_substitutions.h"
#include "string_utils.h"
#include "test_helpers.h"

CLI::App *RenameCommand::CreateCommandCLI(CLI::App &app) {
    auto rename = app.add_subcommand("rename", R"aa("Various commands for renaming files.

This command can be used to bulk rename a set of files. It also has the ability to insert special variables into the file name, such as the detected pitch. As well as this, there is a special auto-mapper command that is useful to sample library developers.

All options for this subcommand relate to just the name of the file - not the folder or the file extension.

Any text added via this command can contain special substitution variables; these will be replaced by values specified in this list:)aa" +
                                                   RenameSubstitution::GetFullInfo());
    rename->require_subcommand();

    auto prefix = rename->add_subcommand("prefix", "Add text to the start of the filename.");
    prefix->add_option("prefix-text", m_prefix, "The text to add, may contain substitution variables.")
        ->required();

    auto suffix =
        rename->add_subcommand("suffix", "Add text to the end of the filename (before the extension).");
    suffix->add_option("suffix-text", m_suffix, "The text to add, may contain substitution variables.")
        ->required();

    auto regex_replace = rename->add_subcommand(
        "regex-replace", "Replace names that match the given regex pattern. The replacement can contain "
                         "regex-groups from the matched filename.");
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

    m_note_to_midi_processor.CreateCLI(*rename);
    m_auto_mapper.CreateCLI(*rename);

    return rename;
}

void RenameCommand::ProcessFiles(AudioFiles &files) {
    m_auto_mapper.InitialiseProcessing(files);

    for (auto [folder, folder_files] : files.Folders()) {
        for (auto &f : folder_files) {
            std::string filename = GetJustFilenameWithNoExtension(f->GetPath());
            bool renamed = false;

            renamed = m_auto_mapper.Rename(*f, folder, filename) | renamed;

            if (m_regex_pattern) {
                const std::regex r {*m_regex_pattern};
                std::smatch pieces_match;
                if (std::regex_match(filename, pieces_match, r)) {
                    auto replacement = m_regex_replacement;
                    for (size_t i = 0; i < pieces_match.size(); ++i) {
                        const std::ssub_match sub_match = pieces_match[i];
                        const std::string piece = sub_match.str();
                        Replace(replacement, PutNumberInAngleBracket(i), piece);
                    }
                    filename = replacement;
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

            renamed = m_note_to_midi_processor.Rename(*f, filename) | renamed;

            if (renamed) {
                if (Contains(filename, "<counter>") || Contains(filename, "<alpha-counter>")) {
                    Replace(filename, "<counter>", std::to_string(m_counter));
                    if (const auto alpha_counter = Get3CharAlphaIdentifier(m_counter)) {
                        Replace(filename, "<alpha-counter>", *alpha_counter);
                    } else {
                        Replace(filename, "<alpha-counter>", std::to_string(m_counter));
                    }
                    m_counter++;
                }
                if (Contains(filename, "<detected-pitch>") || Contains(filename, "<detected-midi-note>") ||
                    Contains(filename, "<detected-note>") ||
                    Contains(filename, "<detected-midi-note-octave-plus-1>") ||
                    Contains(filename, "<detected-midi-note-octave-plus-2>") ||
                    Contains(filename, "<detected-midi-note-octave-minus-1>") ||
                    Contains(filename, "<detected-midi-note-octave-minus-2>") ||
                    Contains(filename, "<detected-midi-note-octaved-to-be-nearest-to-middle-c>")) {
                    if (const auto pitch = f->GetAudio().DetectPitch()) {
                        const auto closest_musical_note = FindClosestMidiPitch(*pitch);

                        Replace(filename, "<detected-pitch>", closest_musical_note.GetPitchString());
                        Replace(filename, "<detected-midi-note>",
                                std::to_string(closest_musical_note.midi_note));
                        Replace(filename, "<detected-midi-note-octave-plus-1>",
                                std::to_string(closest_musical_note.midi_note + 12));
                        Replace(filename, "<detected-midi-note-octave-minus-1>",
                                std::to_string(closest_musical_note.midi_note - 12));
                        Replace(filename, "<detected-midi-note-octave-plus-2>",
                                std::to_string(closest_musical_note.midi_note + 24));
                        Replace(filename, "<detected-midi-note-octave-minus-2>",
                                std::to_string(closest_musical_note.midi_note - 24));
                        Replace(filename, "<detected-note>", closest_musical_note.name);
                        Replace(filename, "<detected-midi-note-octaved-to-be-nearest-to-middle-c>",
                                std::to_string(
                                    ScaleByOctavesToBeNearestToMiddleC(closest_musical_note.midi_note)));
                    } else {
                        WarningWithNewLine(
                            GetName(),
                            "One of the detected pitch variables was used in the file name, but we could not find any pitch in the audio. All detected pitch variables will be replaced with nothing.");
                        Replace(filename, "<detected-pitch>", "");
                        Replace(filename, "<detected-midi-note>", "");
                        Replace(filename, "<detected-midi-note-octave-plus-1>", "");
                        Replace(filename, "<detected-midi-note-octave-minus-1>", "");
                        Replace(filename, "<detected-midi-note-octave-plus-2>", "");
                        Replace(filename, "<detected-midi-note-octave-minus-2>", "");
                        Replace(filename, "<detected-note>", "");
                    }
                }
                if (Contains(filename, "<parent-folder>") || Contains(filename, "<parent-folder-snake>") ||
                    Contains(filename, "<parent-folder-camel>")) {
                    if (folder != ".") {
                        Replace(filename, "<parent-folder>", folder.filename().generic_string());
                        Replace(filename, "<parent-folder-snake>",
                                ToSnakeCase(folder.filename().generic_string()));
                        Replace(filename, "<parent-folder-camel>",
                                ToCamelCase(folder.filename().generic_string()));
                    } else {
                        WarningWithNewLine(
                            GetName(),
                            "The file does not have a parent path, but the variable <parent-folder> was used. This will just be replaced by nothing.");
                        Replace(filename, "<parent-folder>", "");
                    }
                }

                const std::regex re {"<\\w+>"};
                auto var_begin = std::sregex_iterator(filename.begin(), filename.end(), re);
                auto var_end = std::sregex_iterator();
                for (std::sregex_iterator i = var_begin; i != var_end; ++i) {
                    ErrorWithNewLine(GetName(),
                                     "{} is not a valid substitution variable. Available options are: \n{}",
                                     i->str(), RenameSubstitution::GetVariableNames());
                    renamed = false;
                }
            }

            if (renamed) {
                auto path = f->GetPath();
                const auto ext = path.extension();
                path.replace_filename(filename);
                path.replace_extension(ext);
                f->SetPath(path);
            }
        }
    }
}

TEST_CASE("RenameCommand") {
    SUBCASE("requires args") {
        REQUIRE_THROWS(TestHelpers::ProcessFilenameWithCommand<RenameCommand>("rename", {}, "file.wav"));
        REQUIRE_THROWS(
            TestHelpers::ProcessFilenameWithCommand<RenameCommand>("rename prefix", {}, "file.wav"));
        REQUIRE_THROWS(
            TestHelpers::ProcessFilenameWithCommand<RenameCommand>("rename suffix", {}, "file.wav"));
        REQUIRE_THROWS(
            TestHelpers::ProcessFilenameWithCommand<RenameCommand>("rename regex-replace", {}, "file.wav"));
    }

    SUBCASE("prefix") {
        SUBCASE("simple") {
            const auto f =
                TestHelpers::ProcessFilenameWithCommand<RenameCommand>("rename prefix _", {}, "file.wav");
            REQUIRE(f);
            REQUIRE(*f == "_file");
        }
    }

    SUBCASE("suffix") {
        SUBCASE("simple") {
            const auto f =
                TestHelpers::ProcessFilenameWithCommand<RenameCommand>("rename suffix _", {}, "file.wav");
            REQUIRE(f);
            REQUIRE(*f == "file_");
        }
    }

    SUBCASE("regex-replace") {
        SUBCASE("replace everything") {
            const auto f = TestHelpers::ProcessFilenameWithCommand<RenameCommand>(
                "rename regex-replace .* new_name", {}, "file.wav");
            REQUIRE(f);
            REQUIRE(*f == "new_name");
        }
        SUBCASE("insert groups into replacement") {
            const auto f = TestHelpers::ProcessFilenameWithCommand<RenameCommand>(
                "rename regex-replace ([^l]*)(le) <1><2><1>", {}, "file.wav");
            REQUIRE(f);
            REQUIRE(*f == "filefi");
        }
    }

    SUBCASE("note-to-midi") {
        SUBCASE("simple single replace") {
            const auto f = TestHelpers::ProcessFilenameWithCommand<RenameCommand>("rename note-to-midi", {},
                                                                                  "file_c-1.wav");
            REQUIRE(f);
            REQUIRE(*f == "file_0");
        }
        SUBCASE("simple multiple replace") {
            const auto f = TestHelpers::ProcessFilenameWithCommand<RenameCommand>("rename note-to-midi", {},
                                                                                  "file_c-1_C4.wav");
            REQUIRE(f);
            REQUIRE(*f == "file_0_60");
        }
        SUBCASE("simple single replace with altered C-2 for zero num") {
            SUBCASE("c-2 is 0") {
                const auto f = TestHelpers::ProcessFilenameWithCommand<RenameCommand>(
                    "rename note-to-midi --midi-zero-note=C-2", {}, "file_c-2.wav");
                REQUIRE(f);
                REQUIRE(*f == "file_0");
            }
            SUBCASE("c3 is 60") {
                const auto f = TestHelpers::ProcessFilenameWithCommand<RenameCommand>(
                    "rename note-to-midi --midi-zero-note=C-2", {}, "file_c3.wav");
                REQUIRE(f);
                REQUIRE(*f == "file_60");
            }
        }
    }

    SUBCASE("invalid rename substitution") {
        const auto f = TestHelpers::ProcessFilenameWithCommand<RenameCommand>("rename prefix <foo><bar>", {},
                                                                              "foo/file.wav");
        REQUIRE(!f);
    }

    SUBCASE("<parent-folder>") {
        SUBCASE("prefix") {
            const auto f = TestHelpers::ProcessFilenameWithCommand<RenameCommand>(
                "rename prefix <parent-folder>bar", {}, "foo/file.wav");
            REQUIRE(f);
            REQUIRE(*f == "foobarfile");
        }
        SUBCASE("suffix") {
            const auto f = TestHelpers::ProcessFilenameWithCommand<RenameCommand>(
                "rename suffix _<parent-folder-snake>_suf", {}, "folder name/file.wav");
            REQUIRE(f);
            REQUIRE(*f == "file_folder_name_suf");
        }
        SUBCASE("regex-replace") {
            const auto f = TestHelpers::ProcessFilenameWithCommand<RenameCommand>(
                "rename regex-replace .* <0><parent-folder-camel>", {}, "not relavent/foo/file.wav");
            REQUIRE(f);
            REQUIRE(*f == "fileFoo");
        }
    }
}
