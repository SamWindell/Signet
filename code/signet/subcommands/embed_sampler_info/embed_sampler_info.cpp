#include "embed_sampler_info.h"

#include <regex>

#include "CLI11.hpp"

#include "midi_pitches.h"
#include "subcommands/pitch_detector/pitch_detector.h"

std::optional<int> GetIntIfValid(std::string_view str) {
    bool is_int = true;
    for (auto c : str) {
        if (!std::isdigit(c)) {
            is_int = false;
            break;
        }
    }
    if (is_int) {
        return std::atoi(str.data());
    }
    return {};
}

void CLIValidateIntArgIsInRange(const std::string &arg_description, int arg, int min, int max) {
    if (arg < min || arg > max) {
        throw CLI::ValidationError(arg_description, "integer is not in the range (" + std::to_string(min) +
                                                        ", " + std::to_string(max) + ").");
    }
}

bool IsRegexString(std::string_view str) { return Contains(str, "(") && Contains(str, ")"); }

void CLISetArg(const std::string &arg_description,
               const std::string &arg_string,
               int min_int_value,
               std::optional<int> &int_value,
               std::optional<std::string> &regex_pattern) {
    if (auto o = GetIntIfValid(arg_string)) {
        CLIValidateIntArgIsInRange(arg_description, o.value(), min_int_value, 127);
        int_value = o.value();
    } else if (IsRegexString(arg_string)) {
        regex_pattern = arg_string;
    } else if (arg_string == "unchanged") {
    } else {
        throw CLI::ValidationError("Low note", "Argument is not an integer, 'unchanged' or a regex "
                                               "pattern to match against the filename.");
    }
};

const std::string_view g_detected_pitch_options[] = {
    "auto-detect",
    "auto-detect-octave-plus-1",
    "auto-detect-octave-plus-2",
    "auto-detect-octave-minus-1",
    "auto-detect-octave-minus-2",
    "auto-detect-nearest-to-middle-c",
};

std::string GetAllDetectPitchOptions() {
    std::string result {};
    for (usize i = 0; i < std::size(g_detected_pitch_options); ++i) {
        result.append(g_detected_pitch_options[i]);
        if (i != std::size(g_detected_pitch_options) - 1) {
            result.append(", ");
        }
    }
    return result;
}

CLI::App *EmbedSamplerInfo::CreateSubcommandCLI(CLI::App &app) {
    auto embedder = app.add_subcommand(
        "embed-sampler-info", "Sample Info Embed: embeds sampler metadata into the audio file(s), such as "
                              "the root note, the velocity mapping range and the note mapping range.");
    embedder->require_subcommand();

    auto root = embedder->add_subcommand("root", "Embed the root note of the audio file");
    root->add_option_function<std::string>(
            "note",
            [this](const std::string &str) {
                if (auto o = GetIntIfValid(str)) {
                    CLIValidateIntArgIsInRange("MIDI root note", o.value(), 0, 127);
                    m_root_number = o.value();
                    return;
                } else {
                    // Check for auto-detect
                    for (const auto &v : g_detected_pitch_options) {
                        if (str == v) {
                            m_root_auto_detect_name = v;
                            return;
                        }
                    }

                    if (IsRegexString(str)) {
                        m_root_regex_pattern = str;
                        return;
                    }
                }
                throw CLI::ValidationError("Root note",
                                           "This value must either be an MIDI note number integer, a regex "
                                           "pattern capturing a single group to represent the root note in "
                                           "the filename, or a special auto-detect variable.");
            },
            "The root note which is either a MIDI note number, a regex pattern capturing a single "
            "group to represent the root note in the filename, or a special auto-detect variable. The "
            "auto-detect option means the value is determined by attempting to detect the pitch from the "
            "audio file. There are a few different auto-detect options which all use the algorithm, but some "
            "will also do octave-shifts. Here is the complete list of auto-detect options: " +
                GetAllDetectPitchOptions())
        ->required();

    auto note_range = embedder->add_subcommand("note-range", "Embed the low and high notes.");
    note_range
        ->add_option_function<std::vector<std::string>>(
            "args",
            [this](const std::vector<std::string> &args) {
                if (args.size() == 1) {
                    const std::string auto_map_string {"auto-map"};
                    if (args[0] == auto_map_string) {
                        m_note_range_auto_map = true;
                        return;
                    } else {
                        throw CLI::ValidationError(
                            "Note range",
                            "Expected either \"" + std::string(auto_map_string) +
                                "\" or 2 arguments to represent the values for the low and high notes.");
                    }
                }
                assert(args.size() == 2);
                CLISetArg("MIDI low note number", args[0], 0, m_low_note_number, m_low_note_regex_pattern);
                CLISetArg("MIDI high note number", args[1], 0, m_high_note_number, m_high_note_regex_pattern);
            },
            R"aa(This value is either 'auto-map' or 2 separate values. 

EXAMPLES

'note-range auto-map'
Auto maps the file based on the files in its same folder.

'note-range 40 62'
Sets the low note to MIDI note 40 and the high note to 62.

'note-range unchanged 127'
Leaves the low note unchanged (if no value exists already it will be set to 0), and sets the high note to 127.

'note-range "sample_(\d+)_\d+_\d+" "sample_\d+_\d+_(\d+)"'
Sets the low value from the name of the file by pattern-matching the given regex pattern to it, and taking the value of the given capture group (with is the bit inside the brackets). The high value is set from the filename too, but a different regex pattern is matched against. 


DESCRIPTION

If it's auto-map, then the low and high note will be set by assessing the root note of every file in the same folder and calculating an appropriate value. 

If not 'auto-map' then each of the 2 arguments can be 1 of 3 different formats. The first of these 2 arguments represents the low note, and the second represents the high note. The 3 possible formats are as follows: (1) a MIDI note number, (2) a regex pattern containing 1 capture group which is to be used to capture the value from the filename of the audio file (not including the extension). Or (3), the word 'unchanged' which means the value is not changed if it is already embedded in the file; if there is no value already present, it's set to 0 for the low note or 127 for the high note.)aa")
        ->expected(1, 2)
        ->required();

    auto velocity_range = embedder->add_subcommand(
        "velocity-range",
        "Embeds the velocity mapping info. Samplers can often play different samples depending on the MIDI "
        "velocity. In order to to this, the sampler needs to know what the minimum and maximum MIDI velocity "
        "values that a sample should play in. The whole MIDI velocity range is between 1 and 127.");
    velocity_range
        ->add_option_function<std::vector<std::string>>(
            "args",
            [this](const std::vector<std::string> &args) {
                CLISetArg("Velocity low number", args[0], 1, m_low_velo_number, m_low_velo_regex_pattern);
                CLISetArg("Velocity high number", args[1], 1, m_high_velo_number, m_high_velo_regex_pattern);
            },
            R"aa(2 values to represent the low and high velocity mapping. 

EXAMPLES

'velocity-range 1 64'
Sets the low velocity to 1 and the high velocity to 64.

'velocity-range "sample_(\d+)_\d+_\d+" "sample_\d+_\d+_(\d+)"'
Sets the low velocity from the name of the file, by matching the given regex pattern to it, and get the number from the captured region. Same is done for the high velocity but a different capture region is specified.

'velocity-range 1 unchanged'
Sets the low velocity to 1 and leaves the high velocity unchanged. If the high velocity is not already embedding the file, it will be set to 127

DESCRIPTION

2 values must be given. The first one represents the low velocity and the second one represents the high velocity. Each value can be 1 of 3 formats. (1) A number from 1 to 127, (2) a regex pattern containing 1 capture group which is to be used to capture the value from the filename of the audio file (not including the extension). Or (3), the word 'unchanged' which means the value is not changed if it is already embedded in the file; if there is no value already present, it's set to 1 for the low velocity or 127 for the high velocity.)aa")
        ->expected(2)
        ->required();

    return embedder;
}

void EmbedSamplerInfo::ProcessFiles(const tcb::span<EditTrackedAudioFile> files) {
    for (auto &f : files) {
        const auto filename = GetJustFilenameWithNoExtension(f.GetPath());
        auto &metadata = f.GetWritableAudio().metadata;
        if (!metadata.midi_mapping) metadata.midi_mapping.emplace();

        if (!metadata.midi_mapping->sampler_mapping) {
            metadata.midi_mapping->sampler_mapping.emplace();
        }
        auto &sampler_mapping = metadata.midi_mapping->sampler_mapping;

        auto SetFromFilenameRegexMatch = [&](const std::string &pattern, int &out) {
            DebugWithNewLine(pattern);
            DebugWithNewLine(filename);
            const std::regex r {pattern};
            std::smatch pieces_match;
            if (std::regex_match(filename, pieces_match, r)) {
                assert(pieces_match.size() == 1); // TODO: proper error checking
                auto o = GetIntIfValid(pieces_match.str());
                assert(o.has_value()); // TODO: proper error checking
                out = o.value();
            }
        };

        // Root number
        if (m_root_number) {
            metadata.midi_mapping->root_midi_note = m_root_number.value();
        } else if (m_root_regex_pattern) {
            SetFromFilenameRegexMatch(m_root_regex_pattern.value(), metadata.midi_mapping->root_midi_note);
        } else if (m_root_auto_detect_name) {
            auto pitch = PitchDetector::DetectPitch(f.GetAudio());
            assert(pitch); // TODO: proper error checking
            const auto closest_musical_note = FindClosestMidiPitch(*pitch);

            if (m_root_auto_detect_name == "detected") {
                metadata.midi_mapping->root_midi_note = closest_musical_note.midi_note;
            } else if (m_root_auto_detect_name == "detected-octave-plus-1") {
                metadata.midi_mapping->root_midi_note = std::min(closest_musical_note.midi_note + 12, 127);
            } else if (m_root_auto_detect_name == "detected-octave-plus-2") {
                metadata.midi_mapping->root_midi_note = std::min(closest_musical_note.midi_note + 24, 127);
            } else if (m_root_auto_detect_name == "detected-octave-minus-1") {
                metadata.midi_mapping->root_midi_note = std::max(closest_musical_note.midi_note - 12, 0);
            } else if (m_root_auto_detect_name == "detected-octave-minus-2") {
                metadata.midi_mapping->root_midi_note = std::max(closest_musical_note.midi_note - 24, 0);
            } else if (m_root_auto_detect_name == "detected-nearest-to-middle-c") {
                metadata.midi_mapping->root_midi_note =
                    ScaleByOctavesToBeNearestToMiddleC(closest_musical_note.midi_note);
            }
        }

        // Note range, the auto-map case is handled in the ProcessFolders callback
        if (!m_note_range_auto_map) {
            if (m_low_note_number) {
                sampler_mapping->low_note = m_low_note_number.value();
            } else if (m_low_note_regex_pattern) {
                SetFromFilenameRegexMatch(m_low_note_regex_pattern.value(), sampler_mapping->low_note);
            }

            if (m_high_note_number) {
                sampler_mapping->high_note = m_high_note_number.value();
            } else if (m_high_note_regex_pattern) {
                SetFromFilenameRegexMatch(m_high_note_regex_pattern.value(), sampler_mapping->high_note);
            }
        }

        // Velocity range
        if (m_low_velo_number) {
            sampler_mapping->low_velocity = m_low_velo_number.value();
        } else if (m_low_velo_regex_pattern) {
            SetFromFilenameRegexMatch(m_low_velo_regex_pattern.value(), sampler_mapping->low_velocity);
        }
        if (m_high_velo_number) {
            sampler_mapping->high_velocity = m_high_velo_number.value();
        } else if (m_high_velo_regex_pattern) {
            SetFromFilenameRegexMatch(m_high_velo_regex_pattern.value(), sampler_mapping->high_velocity);
        }
    }
}

void EmbedSamplerInfo::ProcessFolders(const FolderMapType &folders) {
    if (m_note_range_auto_map) {
        for (auto &[parent_path, files] : folders) {
            auto sorted_files = files;
            std::sort(sorted_files.begin(), sorted_files.end(), [](const auto &a, const auto &b) {
                return a->GetAudio().metadata.midi_mapping->root_midi_note <
                       b->GetAudio().metadata.midi_mapping->root_midi_note;
            });

            if (sorted_files.size() == 1) {
                sorted_files[0]->GetWritableAudio().metadata.midi_mapping->sampler_mapping->low_note = 0;
                sorted_files[0]->GetWritableAudio().metadata.midi_mapping->sampler_mapping->high_note = 127;
            } else {
                struct MappingData {
                    int root;
                    int low;
                    int high;
                };

                auto GetMappingData = [](EditTrackedAudioFile &f) -> MappingData {
                    return {
                        f.GetAudio().metadata.midi_mapping->root_midi_note,
                        f.GetAudio().metadata.midi_mapping->sampler_mapping->low_note,
                        f.GetAudio().metadata.midi_mapping->sampler_mapping->high_note,
                    };
                };

                auto MapFile = [&](EditTrackedAudioFile &f, MappingData prev, MappingData next) {
                    auto this_data = GetMappingData(f);
                    f.GetWritableAudio().metadata.midi_mapping->sampler_mapping->low_note = prev.high + 1;
                    f.GetWritableAudio().metadata.midi_mapping->sampler_mapping->high_note =
                        this_data.root + (next.root - this_data.root) / 2;
                };

                for (usize i = 0; i < sorted_files.size(); ++i) {
                    if (i == 0) {
                        MapFile(*sorted_files[i], {-1, -1, -1}, GetMappingData(*sorted_files[i + 1]));
                    } else if (i == sorted_files.size() - 1) {
                        MapFile(*sorted_files[i], GetMappingData(*sorted_files[i - 1]), {128, 128, 128});
                    } else {
                        MapFile(*sorted_files[i], GetMappingData(*sorted_files[i - 1]),
                                GetMappingData(*sorted_files[i + 1]));
                    }
                }
                sorted_files.back()->GetWritableAudio().metadata.midi_mapping->sampler_mapping->high_note =
                    127;
            }
        }
    }
}
