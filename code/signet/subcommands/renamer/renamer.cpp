#include "renamer.h"

#include <regex>

#include "audio_file.h"
#include "common.h"
#include "midi_pitches.h"
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

static bool RegexReplaceString(std::string &str, std::string pattern, std::string replacement) {
    const std::regex r {pattern};
    const auto result = std::regex_replace(str, r, replacement);
    if (result != str) {
        str = result;
        return true;
    }
    return false;
}

static std::string MakeNumberInAngleBracket(usize num) { return "<" + std::to_string(num) + ">"; }

bool Renamer::ProcessFilename(const AudioFile &input,
                              std::string &filename,
                              const ghc::filesystem::path &full_path) {
    bool renamed = false;
    if (m_regex_pattern) {
        const std::regex r {*m_regex_pattern};
        std::smatch pieces_match;
        if (std::regex_match(filename, pieces_match, r)) {
            for (size_t i = 0; i < pieces_match.size(); ++i) {
                std::ssub_match sub_match = pieces_match[i];
                std::string piece = sub_match.str();
                std::cout << "  submatch " << i << ": " << piece << '\n';
                RegexReplaceString(m_regex_replacement, MakeNumberInAngleBracket(i), piece);
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
            RegexReplaceString(filename, "<counter>", std::to_string(m_counter++));
        }
        if (Contains(filename, "<detected-pitch>") || Contains(filename, "<detected-midi-node>") ||
            Contains(filename, "<detected-note>")) {
            if (const auto pitch = PitchDetector::DetectPitch(input)) {
                const auto closest_musical_note = FindClosestMidiPitch(*pitch);

                RegexReplaceString(filename, "<detected-pitch>", closest_musical_note.GetPitchString());
                RegexReplaceString(filename, "<detected-midi-note>",
                                   std::to_string(closest_musical_note.midi_note));
                RegexReplaceString(filename, "<detected-note>", closest_musical_note.name);
            } else {
                WarningWithNewLine(
                    "Renamer: One of the detected pitch variables was used in the file name, but we "
                    "could not find any pitch in the audio. All detected pitch variables will be replaced "
                    "with NOPITCH.");
                RegexReplaceString(filename, "<detected-pitch>", "NOPITCH");
                RegexReplaceString(filename, "<detected-midi-note>", "NOPITCH");
                RegexReplaceString(filename, "<detected-note>", "NOPITCH");
            }
        }
        if (Contains(filename, "<parent-folder>")) {
            bool replaced = false;
            if (full_path.has_parent_path()) {
                const auto parent_folder = full_path.parent_path().filename();
                if (parent_folder != ".") {
                    RegexReplaceString(filename, "<parent-folder>", parent_folder.generic_string());
                    replaced = true;
                }
            }
            if (!replaced) {
                WarningWithNewLine("Renamer: The file does not have a parent path, but the variable "
                                   "<parent-folder> was used. This will just be replaced by nothing.");
                RegexReplaceString(filename, "<parent-folder>", "");
            }
        }
    }

    return renamed;
}
