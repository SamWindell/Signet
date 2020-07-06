#include "renamer.h"

#include <regex>

#include "audio_file.h"
#include "common.h"
#include "midi_pitches.h"
#include "string_utils.h"
#include "subcommands/pitch_detector/pitch_detector.h"
#include "test_helpers.h"

static const std::string replacement_variables_info = R"foo(

<counter>
A unique number starting from zero. The ordering of these numbers is not specified.

<detected-pitch>
The detected pitch of audio file in Hz. If no pitch is found this variable will be empty.

<detected-midi-note>
The MIDI note number that is closest to the detected pitch of the audio file. If no pitch is found this variable will be empty.

<detected-note>
The musical note-name that is closest to the detected pitch of the audio file. The note is capitalised, and the octave number is specified. For example 'C3'. If no pitch is found this variable will be empty.

<parent-folder>
The name of the folder that contains the audio file.

<parent-folder-snake>
The snake-case name of the folder that contains the audio file.

<parent-folder-camel>
The camel-case name of the folder that contains the audio file.)foo";

CLI::App *Renamer::CreateSubcommandCLI(CLI::App &app) {
    auto renamer = app.add_subcommand("rename",
                                      R"aa(File Renamer: various commands for renaming files.

This command can be used to bulk rename a set of files. It also has the ability to insert special variables into the file name, such as the detected pitch. As well as this, there is a special auto-mapper command that is useful to sample library developers.

All options for this subcommand relate to just the name of the file - not the folder or the file extension.

Any text added via this command can contain special substitution variables; these will be replaced by values specified in this list: )aa" +
                                          replacement_variables_info);
    renamer->require_subcommand();

    auto prefix = renamer->add_subcommand("prefix", "Add text to the start of the filename.");
    prefix->add_option("prefix-text", m_prefix, "The text to add, may contain substitution variables.")
        ->required();

    auto suffix =
        renamer->add_subcommand("suffix", "Add text to the end of the filename (before the extension).");
    suffix->add_option("suffix-text", m_suffix, "The text to add, may contain substitution variables.")
        ->required();

    auto regex_replace = renamer->add_subcommand(
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

    auto note_to_midi = renamer->add_subcommand(
        "note-to-midi", "Replace all occurrences of note names with the corresponding MIDI note number. For "
                        "example replace C3 with 60.");
    note_to_midi->final_callback([this] { m_note_to_midi = true; });
    note_to_midi->add_option("--midi-zero-note", m_midi_0_note,
                             "The note that should represent MIDI note number 0. Default is C-1.");

    auto auto_map = renamer->add_subcommand(
        "auto-map",
        R"^^(Samplers can often read the root, low and high MIDI note numbers from within a filename. This command makes inserting the low and high keys into the filename simple.

First you specify a regex pattern that captures a number representing the MIDI root note from the input filenames. This tool collects all of the root notes found in each folder, and works out reasonable values for what the low and high MIDI notes should be.

You control the format of the renaming by specifing a pattern containing substitution variables for <lo>, <root> and <high>. These variables are replaced by the MIDI note numbers in the range 0 to 127.)^^");

    auto_map
        ->add_option("auto-map-filename-root-note-pattern", m_automap_pattern,
                     "The ECMAScript-style regex the should match against filename (excluding extension). "
                     "This regex should contain 1 capture group to represent the root note of the sample.")
        ->required();

    auto_map
        ->add_option(
            "auto-map-renamed-filename", m_automap_out,
            "The name of the output file (excluding extension). This should contain substitution variables "
            "<lo>, <root> and <hi> which will be replaced by the low MIDI note number, the root MIDI note "
            "number and the high MIDI note number. The low and high numbers are generated by the auto-mapper "
            "so that all samples in each folder will fill out the entire range 0-127.")
        ->required();

    return renamer;
}

void Renamer::AddToFolderMap(const fs::path &path) {
    REQUIRE(m_automap_pattern);
    if (!path.has_parent_path()) {
        REQUIRE(false); // expecting canonical paths
        return;
    }

    const std::string filename = GetJustFilenameWithNoExtension(path);
    std::smatch pieces_match;
    std::regex r {*m_automap_pattern};
    if (std::regex_match(filename, pieces_match, r)) {
        if (pieces_match.size() != 2) {
            ErrorWithNewLine("Renamer: the regex pattern does not contain just 1 group ", *m_automap_pattern);
            return;
        }

        const auto root_note = std::stoi(pieces_match[1]);
        if (root_note < 0 || root_note > 127) {
            WarningWithNewLine("Renamer: root note of file ", filename,
                               " is not in the range 0-127 so cannot be processed");
        } else {
            MessageWithNewLine("Renamer", "automap found root note ", root_note, " in filename ", path);
            const auto parent = path.parent_path().generic_string();
            m_folder_map[parent].AddFile(path, root_note);
        }
    }
}

void Renamer::ConstructAllAutomappings() {
    for (auto &[path, folder] : m_folder_map) {
        folder.Automap();
    }
}

static const std::array<std::string_view, 12> g_notes {"c",  "c#", "d",  "d#", "e",  "f",
                                                       "f#", "g",  "g#", "a",  "a#", "b"};

struct Note {
    std::string_view note_letter;
    int note_index;
    int octave;
};

static int SemitoneDistance(const Note &a, const Note &b) {
    const auto octave_distance = b.octave - a.octave;
    const auto note_distance = b.note_index - a.note_index;
    return octave_distance * 12 + note_distance;
}

static std::optional<Note> ParseNote(std::string note_string) {
    Lowercase(note_string);
    Note note {};
    for (usize i = 0; i < g_notes.size(); ++i) {
        if (StartsWith(note_string, g_notes[i])) {
            note.note_index = (int)i;
            note.note_letter = g_notes[i];
        }
    }
    if (note.note_letter.size() == 0) {
        return {};
    }
    try {
        note.octave = std::stoi(note_string.substr(note.note_letter.size()));
    } catch (...) {
        return {};
    }
    return note;
}

static std::optional<std::string_view> FindNoteName(std::string_view str) {
    for (usize i = 0; i < str.size(); ++i) {
        auto pos = i;
        if (pos == 0 || !std::isalnum(str[pos])) {
            if (pos != 0) pos++;
            if (pos < str.size()) {
                if ((str[pos] >= 'a' && str[pos] <= 'g') || (str[pos] >= 'A' && str[pos] <= 'G')) {
                    const auto start_index = pos;
                    pos++;
                    if (pos < str.size()) {
                        if (str[pos] == '#') {
                            pos++;
                        }
                        if (pos < str.size()) {
                            if (str[pos] == '-') {
                                pos++;
                            }
                            if (pos < str.size()) {
                                if (std::isdigit(str[pos])) {
                                    pos++;
                                    const auto end_index = pos;
                                    if (pos == str.size() || !std::isalnum(str[pos])) {
                                        return str.substr(start_index, end_index - start_index);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return {};
}

void Renamer::ProcessFiles(const tcb::span<EditTrackedAudioFile> files) {
    if (m_automap_pattern) {
        for (auto &f : files) {
            AddToFolderMap(f.GetPath());
        }
        ConstructAllAutomappings();
    }

    for (auto &f : files) {
        std::string filename = GetJustFilenameWithNoExtension(f.GetPath());
        bool renamed = false;

        if (m_automap_pattern) {
            auto &folder = m_folder_map[f.GetPath().parent_path().generic_string()];
            if (const auto file = folder.GetFile(f.GetPath())) {
                auto new_name = *m_automap_out;
                Replace(new_name, "<lo>", std::to_string(file->low));
                Replace(new_name, "<hi>", std::to_string(file->high));
                Replace(new_name, "<root>", std::to_string(file->root));
                filename = new_name;
                renamed = true;
            }
        }

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

        if (m_note_to_midi) {
            const auto zero_note = ParseNote(m_midi_0_note);
            if (!zero_note) {
                WarningWithNewLine("Renamer note-to-midi: given root note is not valid: ", m_midi_0_note);
            } else {
                while (const auto note_str = FindNoteName(filename)) {
                    const auto note = ParseNote(std::string(*note_str));
                    if (!note) continue;
                    const auto replacement = std::to_string(SemitoneDistance(*zero_note, *note));
                    Replace(filename, std::string(*note_str), replacement);
                    renamed = true;
                }
            }
        }

        if (renamed) {
            if (Contains(filename, "<counter>")) {
                Replace(filename, "<counter>", std::to_string(m_counter++));
            }
            if (Contains(filename, "<detected-pitch>") || Contains(filename, "<detected-midi-note>") ||
                Contains(filename, "<detected-note>")) {
                if (const auto pitch = PitchDetector::DetectPitch(f.GetAudio())) {
                    const auto closest_musical_note = FindClosestMidiPitch(*pitch);

                    Replace(filename, "<detected-pitch>", closest_musical_note.GetPitchString());
                    Replace(filename, "<detected-midi-note>", std::to_string(closest_musical_note.midi_note));
                    Replace(filename, "<detected-note>", closest_musical_note.name);
                } else {
                    WarningWithNewLine(
                        "Renamer: One of the detected pitch variables was used in the file name, but we "
                        "could not find any pitch in the audio. All detected pitch variables will be "
                        "replaced "
                        "substituted with nothing.");
                    Replace(filename, "<detected-pitch>", "");
                    Replace(filename, "<detected-midi-note>", "");
                    Replace(filename, "<detected-note>", "");
                }
            }
            if (Contains(filename, "<parent-folder>") || Contains(filename, "<parent-folder-snake>") ||
                Contains(filename, "<parent-folder-camel>")) {
                bool replaced = false;
                if (f.GetPath().has_parent_path()) {
                    const auto parent_folder = f.GetPath().parent_path().filename();
                    if (parent_folder != ".") {
                        const auto folder = parent_folder.generic_string();
                        Replace(filename, "<parent-folder>", folder);
                        Replace(filename, "<parent-folder-snake>", ToSnakeCase(folder));
                        Replace(filename, "<parent-folder-camel>", ToCamelCase(folder));
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
            auto path = f.GetPath();
            const auto ext = path.extension();
            path.replace_filename(filename);
            path.replace_extension(ext);
            f.SetPath(path);
        }
    }
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

    SUBCASE("note-to-midi") {
        SUBCASE("simple single replace") {
            const auto f = TestHelpers::ProcessFilenameWithSubcommand<Renamer>("rename note-to-midi", {},
                                                                               "file_c-1.wav");
            REQUIRE(f);
            REQUIRE(*f == "file_0");
        }
        SUBCASE("simple multiple replace") {
            const auto f = TestHelpers::ProcessFilenameWithSubcommand<Renamer>("rename note-to-midi", {},
                                                                               "file_c-1_C4.wav");
            REQUIRE(f);
            REQUIRE(*f == "file_0_60");
        }
        SUBCASE("simple single replace with altered C-2 for zero num") {
            SUBCASE("c-2 is 0") {
                const auto f = TestHelpers::ProcessFilenameWithSubcommand<Renamer>(
                    "rename note-to-midi --midi-zero-note=C-2", {}, "file_c-2.wav");
                REQUIRE(f);
                REQUIRE(*f == "file_0");
            }
            SUBCASE("c3 is 60") {
                const auto f = TestHelpers::ProcessFilenameWithSubcommand<Renamer>(
                    "rename note-to-midi --midi-zero-note=C-2", {}, "file_c3.wav");
                REQUIRE(f);
                REQUIRE(*f == "file_60");
            }
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
                "rename suffix _<parent-folder-snake>_suf", {}, "folder name/file.wav");
            REQUIRE(f);
            REQUIRE(*f == "file_folder_name_suf");
        }
        SUBCASE("regex-replace") {
            const auto f = TestHelpers::ProcessFilenameWithSubcommand<Renamer>(
                "rename regex-replace .* <0><parent-folder-camel>", {}, "not relavent/foo/file.wav");
            REQUIRE(f);
            REQUIRE(*f == "fileFoo");
        }
    }

    SUBCASE("note functions") {
        SUBCASE("parse from string") {
            SUBCASE("lower") {
                const auto r = ParseNote("c1");
                REQUIRE(r);
                REQUIRE(r->note_letter == "c");
                REQUIRE(r->note_index == 0);
                REQUIRE(r->octave == 1);
            }
            SUBCASE("upper sharp negative octave") {
                const auto r = ParseNote("G#-1");
                REQUIRE(r);
                REQUIRE(r->note_letter == "g#");
                REQUIRE(r->note_index == 8);
                REQUIRE(r->octave == -1);
            }
            SUBCASE("fail with incorrect note") { REQUIRE(!ParseNote("z0")); }
            SUBCASE("fail with missing octave") { REQUIRE(!ParseNote("c")); }
            SUBCASE("fail with empty string") { REQUIRE(!ParseNote("")); }
        }
        SUBCASE("distance") {
            auto GetDistance = [&](auto a, auto b) { return SemitoneDistance(*ParseNote(a), *ParseNote(b)); };
            REQUIRE(GetDistance("C0", "C#0") == 1);
            REQUIRE(GetDistance("C#0", "C0") == -1);
            REQUIRE(GetDistance("C0", "C1") == 12);
            REQUIRE(GetDistance("C1", "C0") == -12);
            REQUIRE(GetDistance("C1", "B0") == -1);
            REQUIRE(GetDistance("B0", "C1") == 1);
            REQUIRE(GetDistance("B-2", "B2") == 48);
        }
        SUBCASE("find note name") {
            auto Exists = [&](std::string_view haystack, std::string_view note_name) {
                const auto r = FindNoteName(haystack);
                return r && *r == note_name;
            };

            REQUIRE(Exists("file_c-1.wav", "c-1"));
            REQUIRE(Exists("file_c1.wav", "c1"));
            REQUIRE(Exists("file_C1.wav", "C1"));
            REQUIRE(Exists("file_g#2.wav", "g#2"));
            REQUIRE(Exists("file_g-2.wav", "g-2"));
            REQUIRE(Exists("file_g#-2.wav", "g#-2"));
            REQUIRE(Exists("file_c1", "c1"));
            REQUIRE(Exists("file_C1", "C1"));
            REQUIRE(Exists("file_g#2", "g#2"));
            REQUIRE(Exists("file_g-2", "g-2"));
            REQUIRE(Exists("file_g#-2", "g#-2"));
            REQUIRE(Exists("c1-g", "c1"));
            REQUIRE(Exists("C#-2", "C#-2"));
            REQUIRE(!Exists("music1", ""));
            REQUIRE(!Exists("c1333", ""));
            REQUIRE(!Exists("c#1333", ""));
            REQUIRE(!Exists("c#-1333", ""));
        }
    }
}
