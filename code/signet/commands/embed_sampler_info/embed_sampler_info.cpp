#include "embed_sampler_info.h"

#include <regex>

#include "CLI11.hpp"
#include "doctest.hpp"
#include <fmt/core.h>

#include "midi_pitches.h"
#include "test_helpers.h"

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

bool IsRegexString(std::string_view str, const std::string &arg_description) {
    std::regex r(str.data(), str.size());
    const auto capture_regions = r.mark_count();
    if (capture_regions == 1) return true;
    if (capture_regions > 1) {
        throw CLI::ValidationError(arg_description, "Argument does not have exactly 1 capture group.");
    }
    return false;
}

void CLISetArg(const std::string &arg_description,
               const std::string &arg_string,
               int min_int_value,
               std::optional<int> &int_value,
               std::optional<std::string> &regex_pattern) {
    if (arg_string == "unchanged") return;

    if (auto o = GetIntIfValid(arg_string)) {
        CLIValidateIntArgIsInRange(arg_description, o.value(), min_int_value, 127);
        int_value = o.value();
    } else if (IsRegexString(arg_string, arg_description)) {
        regex_pattern = arg_string;
    } else {
        throw CLI::ValidationError("Low note", "Argument is not an integer, 'unchanged' or a regex "
                                               "pattern to match against the filename.");
    }
};

const std::string_view g_auto_detect_pitch_options[] = {
    "auto-detect",
    "auto-detect-octave-plus-1",
    "auto-detect-octave-plus-2",
    "auto-detect-octave-minus-1",
    "auto-detect-octave-minus-2",
    "auto-detect-nearest-to-middle-c",
};

std::string GetAllDetectPitchOptions() {
    std::string result {};
    for (usize i = 0; i < std::size(g_auto_detect_pitch_options); ++i) {
        result.append(g_auto_detect_pitch_options[i]);
        if (i != std::size(g_auto_detect_pitch_options) - 1) {
            result.append(", ");
        }
    }
    return result;
}

const std::string_view g_regex_capture_argument_description =
    R"aa(An ECMAScript-style regex pattern containing 1 capture group which is to be used to get the value from the filename of the audio file (not including the extension). For example if the file was called sample_40.wav, you could use the regex pattern "sample_(\d+)" to get the value 40.)aa";

std::string CLIGetNoteArgumentDescription() {
    return fmt::format(R"aa(This value should be 1 of the following 3 formats:

(1) A MIDI note number.
(2) {}
(3) 'auto-detect' or one of the other auto-detect options. This will invoke the pitch-detection algorithm to automatically get the MIDI number from the audio file. There are a few variations for auto-detect. They all use the same algorithm but some also shift the value in octaves. Here is the full list of options: {}.

EXAMPLE

'root 60'
Sets the root note to MIDI number 60.

'root "sample_(\d+)"'
Sets the root note by looking at the filename and matching the given regex pattern to it.

'root auto-detect'
Sets the root note by running a pitch detection algorithm on the file. If the audio file does not have a pitch, the value will be set to 60.)aa",
                       g_regex_capture_argument_description, GetAllDetectPitchOptions());
}

CLI::App *EmbedSamplerInfo::CreateCommandCLI(CLI::App &app) {
    auto embedder = app.add_subcommand(
        "embed-sampler-info", "Embeds sampler metadata into the audio file(s), such as "
                              "the root note, the velocity mapping range and the note mapping range.");
    embedder->require_subcommand();

    auto root = embedder->add_subcommand("root", "Embed the root note of the audio file");
    root->add_option_function<std::string>(
            "Root note value",
            [this](const std::string &str) {
                if (auto o = GetIntIfValid(str)) {
                    CLIValidateIntArgIsInRange("MIDI root note", o.value(), 0, 127);
                    m_root_number = o.value();
                    return;
                } else {
                    // Check for auto-detect
                    for (const auto &v : g_auto_detect_pitch_options) {
                        if (str == v) {
                            m_root_auto_detect_name = v;
                            return;
                        }
                    }

                    if (IsRegexString(str, "Root note")) {
                        m_root_regex_pattern = str;
                        return;
                    }
                }
                throw CLI::ValidationError("Root note",
                                           "This value must either be an MIDI note number integer, a regex "
                                           "pattern capturing a single group to represent the root note in "
                                           "the filename, or a special auto-detect variable.");
            },
            CLIGetNoteArgumentDescription())
        ->required();

    auto note_range = embedder->add_subcommand("note-range", "Embed the low and high notes.");
    note_range
        ->add_option_function<std::vector<std::string>>(
            "Note range value(s)",
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
            R"aa(This value is either 'auto-map' or 2 separate values to set the low and high note range.

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
        "velocity. In order to do this, the sampler needs to know what the minimum and maximum MIDI velocity "
        "values that a sample should play in. The whole MIDI velocity range is between 1 and 127.");
    velocity_range
        ->add_option_function<std::vector<std::string>>(
            "Low and high velocity values",
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

void EmbedSamplerInfo::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        const auto filename = GetJustFilenameWithNoExtension(f.GetPath());
        auto &metadata = f.GetWritableAudio().metadata;
        if (!metadata.midi_mapping) metadata.midi_mapping.emplace();

        if (!metadata.midi_mapping->sampler_mapping) {
            metadata.midi_mapping->sampler_mapping.emplace();
        }
        auto &sampler_mapping = metadata.midi_mapping->sampler_mapping;

        auto SetFromFilenameRegexMatch = [&](const std::string &pattern, int &out) {
            const std::regex r {pattern};
            std::smatch pieces_match;
            if (std::regex_match(filename, pieces_match, r)) {
                assert(pieces_match.size() == 2); // should be validated by the CLI parsing
                auto o = GetIntIfValid(pieces_match[1].str());
                if (!o) {
                    ErrorWithNewLine(
                        GetName(),
                        "The given regex pattern {} does not capture an integer in the filename {}. The value will instead be set to an appropriate default value.",
                        pattern, filename);
                } else {
                    out = o.value();
                }
            } else {
                ErrorWithNewLine(
                    GetName(),
                    "The given regex pattern {} does not match the filename {}, no value could be captured",
                    pattern, filename);
            }
        };

        // Root number
        if (m_root_number) {
            metadata.midi_mapping->root_midi_note = m_root_number.value();
        } else if (m_root_regex_pattern) {
            SetFromFilenameRegexMatch(m_root_regex_pattern.value(), metadata.midi_mapping->root_midi_note);
        } else if (m_root_auto_detect_name) {
            int midi_note = 60;
            if (auto pitch = f.GetAudio().DetectPitch()) {
                midi_note = FindClosestMidiPitch(*pitch).midi_note;
            }

            if (m_root_auto_detect_name == "auto-detect") {
                metadata.midi_mapping->root_midi_note = midi_note;
            } else if (m_root_auto_detect_name == "auto-detect-octave-plus-1") {
                metadata.midi_mapping->root_midi_note = std::min(midi_note + 12, 127);
            } else if (m_root_auto_detect_name == "auto-detect-octave-plus-2") {
                metadata.midi_mapping->root_midi_note = std::min(midi_note + 24, 127);
            } else if (m_root_auto_detect_name == "auto-detect-octave-minus-1") {
                metadata.midi_mapping->root_midi_note = std::max(midi_note - 12, 0);
            } else if (m_root_auto_detect_name == "auto-detect-octave-minus-2") {
                metadata.midi_mapping->root_midi_note = std::max(midi_note - 24, 0);
            } else if (m_root_auto_detect_name == "auto-detect-nearest-to-middle-c") {
                metadata.midi_mapping->root_midi_note = ScaleByOctavesToBeNearestToMiddleC(midi_note);
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

    if (m_note_range_auto_map) {
        for (auto &[parent_path, files_in_folder] : files.Folders()) {
            auto sorted_files = files_in_folder;
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

TEST_CASE("EmbedSamplerInfo") {
    const auto buf = TestHelpers::CreateSquareWaveAtFrequency(1, 44100, 0.2, 440);
    REQUIRE(!buf.metadata.midi_mapping);

    SUBCASE("requires subcommand") {
        REQUIRE_THROWS(TestHelpers::ProcessBufferWithCommand<EmbedSamplerInfo>("embed-sampler-info", buf));
    }

    SUBCASE("accepts multple subcommands") {
        REQUIRE(TestHelpers::ProcessBufferWithCommand<EmbedSamplerInfo>(
            "embed-sampler-info root 60 note-range 60 60 velocity-range 60 60", buf));
    }

    SUBCASE("root note") {
        SUBCASE("sets integer") {
            auto out =
                TestHelpers::ProcessBufferWithCommand<EmbedSamplerInfo>("embed-sampler-info root 60", buf);
            REQUIRE(out);
            REQUIRE(out->metadata.midi_mapping);
            REQUIRE(out->metadata.midi_mapping->root_midi_note == 60);
        }
        SUBCASE("sets auto-detect") {
            auto CheckAutoDetectOption = [&](const std::string_view option, int expected_value) {
                auto out = TestHelpers::ProcessBufferWithCommand<EmbedSamplerInfo>(
                    fmt::format("embed-sampler-info root {}", option), buf);
                CHECK(out);
                CHECK(out->metadata.midi_mapping);
                CHECK(out->metadata.midi_mapping->root_midi_note == expected_value);
            };
            CheckAutoDetectOption("auto-detect", 69);
            CheckAutoDetectOption("auto-detect-octave-plus-1", 69 + 12);
            CheckAutoDetectOption("auto-detect-octave-plus-2", 69 + 24);
            CheckAutoDetectOption("auto-detect-octave-minus-1", 69 - 12);
            CheckAutoDetectOption("auto-detect-octave-minus-2", 69 - 24);
            CheckAutoDetectOption("auto-detect-nearest-to-middle-c", 57);
        }
        SUBCASE("set from filename") {
            auto out = TestHelpers::ProcessBufferWithCommand<EmbedSamplerInfo>(
                "embed-sampler-info root sample_(\\d+)", buf, "sample_60.wav");
            REQUIRE(out);
            REQUIRE(out->metadata.midi_mapping);
            REQUIRE(out->metadata.midi_mapping->root_midi_note == 60);
        }
    }

    SUBCASE("note range") {
        SUBCASE("2 args are required unless auto-map") {
            REQUIRE_THROWS(TestHelpers::ProcessBufferWithCommand<EmbedSamplerInfo>(
                "embed-sampler-info note-range 61", buf));
        }

        SUBCASE("sets integer") {
            auto out = TestHelpers::ProcessBufferWithCommand<EmbedSamplerInfo>(
                "embed-sampler-info note-range 61 62", buf);
            REQUIRE(out);
            REQUIRE(out->metadata.midi_mapping);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping->low_note == 61);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping->high_note == 62);
        }
        SUBCASE("sets from filename") {
            auto out = TestHelpers::ProcessBufferWithCommand<EmbedSamplerInfo>(
                "embed-sampler-info note-range sample_(\\d+)_\\d+ sample_\\d+_(\\d+)", buf,
                "sample_61_62.wav");
            REQUIRE(out);
            REQUIRE(out->metadata.midi_mapping);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping->low_note == 61);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping->high_note == 62);
        }
        SUBCASE("unchanged works") {
            auto out = TestHelpers::ProcessBufferWithCommand<EmbedSamplerInfo>(
                "embed-sampler-info note-range 61 unchanged", buf);
            REQUIRE(out);
            REQUIRE(out->metadata.midi_mapping);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping->low_note == 61);
            const MetadataItems::SamplerMapping default_sampler_mapping_vals {};
            REQUIRE(out->metadata.midi_mapping->sampler_mapping->high_note ==
                    default_sampler_mapping_vals.high_note);
        }

        SUBCASE("sets from auto-map") {
            const auto a3 = TestHelpers::CreateSquareWaveAtFrequency(1, 44100, 0.2, 220);
            const auto a4 = TestHelpers::CreateSquareWaveAtFrequency(1, 44100, 0.2, 440);
            const auto a5 = TestHelpers::CreateSquareWaveAtFrequency(1, 44100, 0.2, 880);

            auto out = TestHelpers::ProcessBuffersWithCommand<EmbedSamplerInfo>(
                "embed-sampler-info root auto-detect note-range auto-map", {a3, a4, a5},
                {"a3.wav", "a4.wav", "a5.wav"});

            REQUIRE(out.size());
            for (const auto &o : out) {
                REQUIRE(o);
                REQUIRE(o->metadata.midi_mapping);
                REQUIRE(o->metadata.midi_mapping->sampler_mapping);
            }

            auto &o_map1 = out[0]->metadata.midi_mapping.value();
            auto &o_map2 = out[1]->metadata.midi_mapping.value();
            auto &o_map3 = out[2]->metadata.midi_mapping.value();

            REQUIRE(o_map1.root_midi_note == 57);
            REQUIRE(o_map2.root_midi_note == 69);
            REQUIRE(o_map3.root_midi_note == 81);

            REQUIRE(o_map1.sampler_mapping->low_note == 0);
            REQUIRE(o_map1.sampler_mapping->high_note < o_map2.sampler_mapping->low_note);
            REQUIRE(o_map3.sampler_mapping->low_note > o_map2.sampler_mapping->high_note);
            REQUIRE(o_map3.sampler_mapping->high_note == 127);
        }
    }

    SUBCASE("velocity range") {
        SUBCASE("2 args are required unless auto-map") {
            REQUIRE_THROWS(TestHelpers::ProcessBufferWithCommand<EmbedSamplerInfo>(
                "embed-sampler-info velocity-range 61", buf));
        }

        SUBCASE("set integer") {
            auto out = TestHelpers::ProcessBufferWithCommand<EmbedSamplerInfo>(
                "embed-sampler-info velocity-range 61 62", buf);
            REQUIRE(out);
            REQUIRE(out->metadata.midi_mapping);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping->low_velocity == 61);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping->high_velocity == 62);
        }

        SUBCASE("sets from filename") {
            auto out = TestHelpers::ProcessBufferWithCommand<EmbedSamplerInfo>(
                "embed-sampler-info velocity-range sample_(\\d+)_\\d+ sample_\\d+_(\\d+)", buf,
                "sample_61_62.wav");
            REQUIRE(out);
            REQUIRE(out->metadata.midi_mapping);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping->low_velocity == 61);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping->high_velocity == 62);
        }

        SUBCASE("unchanged works") {
            auto out = TestHelpers::ProcessBufferWithCommand<EmbedSamplerInfo>(
                "embed-sampler-info velocity-range 61 unchanged", buf);
            REQUIRE(out);
            REQUIRE(out->metadata.midi_mapping);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping);
            REQUIRE(out->metadata.midi_mapping->sampler_mapping->low_velocity == 61);
            const MetadataItems::SamplerMapping default_sampler_mapping_vals {};
            REQUIRE(out->metadata.midi_mapping->sampler_mapping->high_velocity ==
                    default_sampler_mapping_vals.high_velocity);
        }
    }
}
