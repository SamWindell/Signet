#include "print_info.h"

#include "CLI11.hpp"
#include "json.hpp"
#include <cereal/archives/json.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <regex>

#include "doctest.hpp"
#include "gain_calculators.h"
#include "magic_enum.hpp"
#include "midi_pitches.h"
#include "test_helpers.h"

CLI::App *PrintInfoCommand::CreateCommandCLI(CLI::App &app) {
    auto printer = app.add_subcommand(
        "print-info",
        "Prints information about the audio file(s), such as the embedded metadata, sample-rate and RMS.");

    std::map<std::string, PrintInfoCommand::Format> format_name_dictionary;
    for (const auto &e : magic_enum::enum_entries<PrintInfoCommand::Format>()) {
        auto name = std::string(e.second);
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        format_name_dictionary[name] = e.first;
    }
    printer
        ->add_option_function<PrintInfoCommand::Format>(
            "--format",
            [this](Format format) {
                m_format = format;
                switch (format) {
                    case Format::Json:
                    case Format::Lua:
                        // We want stdout to be uncluttered by messages so the user can easily parse the
                        // output.
                        g_messages_enabled = false;
                        break;
                    case Format::Text: break;
                }
            },
            "Output format for the information. Default is text.")
        ->transform(CLI::CheckedTransformer(format_name_dictionary, CLI::ignore_case));

    printer->add_flag("--path-as-key", m_path_as_key,
                      "If set, the path of the files will be used as the keys in the JSON/Lua output. "
                      "Otherwise, it will be an array of objects with 'path' as a field.");

    printer->add_option(
        "--field-filter", m_field_filter_regex,
        "If set, only the fields matching this regex will be printed in the JSON/Lua output. For example, \"(channels|sample_rate)\" will only print the channels and sample_rate fields.");

    printer->add_flag("--detect-pitch", m_detect_pitch,
                      "Detect the pitch of the audio file(s) and print it out.");
    return printer;
}

static bool IsValidLuaIdentifier(const std::string &str) {
    if (str.empty()) return false;

    // Check if first character is letter or underscore
    if (!std::isalpha(str[0]) && str[0] != '_') {
        return false;
    }

    // Check remaining characters are alphanumeric or underscore
    for (size_t i = 1; i < str.size(); ++i) {
        if (!std::isalnum(str[i]) && str[i] != '_') {
            return false;
        }
    }

    // Check if it's a Lua keyword
    static const std::set<std::string> luaKeywords = {
        "and",   "break", "do",  "else", "elseif", "end",    "false", "for",  "function", "if",   "in",
        "local", "nil",   "not", "or",   "repeat", "return", "then",  "true", "until",    "while"};

    return luaKeywords.find(str) == luaKeywords.end();
}

static std::string EscapeLuaString(const std::string &str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default: oss << c; break;
        }
    }
    return oss.str();
}

static std::string JsonToLuaTable(const nlohmann::json &j, int indent = 0) {
    std::ostringstream oss;
    std::string indentStr(indent * 2, ' ');
    std::string nextIndentStr((indent + 1) * 2, ' ');

    if (j.is_null()) {
        oss << "nil";
    } else if (j.is_boolean()) {
        oss << (j.get<bool>() ? "true" : "false");
    } else if (j.is_number_integer()) {
        oss << j.get<int64_t>();
    } else if (j.is_number_float()) {
        oss << j.get<double>();
    } else if (j.is_string()) {
        std::string str = j.get<std::string>();
        oss << "\"";
        oss << EscapeLuaString(str);
        oss << "\"";
    } else if (j.is_array()) {
        oss << "{\n";
        bool first = true;
        for (const auto &element : j) {
            if (!first) {
                oss << ",\n";
            }
            oss << nextIndentStr << JsonToLuaTable(element, indent + 1);
            first = false;
        }
        if (!j.empty()) {
            oss << "\n";
        }
        oss << indentStr << "}";
    } else if (j.is_object()) {
        oss << "{\n";
        bool first = true;
        for (const auto &[key, value] : j.items()) {
            if (!first) {
                oss << ",\n";
            }

            // Handle key formatting - use brackets for non-identifier keys
            if (IsValidLuaIdentifier(key)) {
                oss << nextIndentStr << key << " = ";
            } else {
                oss << nextIndentStr << "[\"" << EscapeLuaString(key) << "\"] = ";
            }

            oss << JsonToLuaTable(value, indent + 1);
            first = false;
        }
        if (!j.empty()) {
            oss << "\n";
        }
        oss << indentStr << "}";
    }

    return oss.str();
}

void PrintInfoCommand::ProcessFiles(AudioFiles &files) {
    if (m_format == Format::Text) {
        for (auto &f : files) {
            std::string info_text;
            if (!f.GetAudio().metadata.IsEmpty()) {
                std::stringstream ss {};
                {
                    try {
                        cereal::JSONOutputArchive archive(ss);
                        archive(cereal::make_nvp("Metadata", f.GetAudio().metadata));
                    } catch (const std::exception &e) {
                        ErrorWithNewLine(GetName(), f, "Internal error when writing fetch the metadata: {}",
                                         e.what());
                    }
                }
                info_text += ss.str() + "\n";
            } else {
                info_text += "Contains no metadata that Signet understands\n";
            }

            info_text += fmt::format("Channels: {}\n", f.GetAudio().num_channels);
            info_text += fmt::format("Sample Rate: {}\n", f.GetAudio().sample_rate);
            info_text += fmt::format("Frames: {}\n", f.GetAudio().NumFrames());
            info_text += fmt::format("Length: {:.2f} seconds\n",
                                     (double)f.GetAudio().NumFrames() / (double)f.GetAudio().sample_rate);
            info_text += fmt::format("Bit-depth: {}\n", f.GetAudio().bits_per_sample);

            auto const rms = GetRMS(f.GetAudio().interleaved_samples);
            auto const peak = GetPeak(f.GetAudio().interleaved_samples);
            auto const crest_factor = peak / rms;
            info_text += fmt::format("RMS: {:.2f} dB\n", AmpToDB(rms));
            info_text += fmt::format("Peak: {:.2f} dB\n", AmpToDB(peak));
            info_text +=
                fmt::format("Crest Factor: {:.2f} dB ({:.2f})\n", AmpToDB(crest_factor), crest_factor);

            if (m_detect_pitch) {
                const auto pitch = f.GetAudio().DetectPitch();
                if (pitch) {
                    const auto closest_musical_note = FindClosestMidiPitch(*pitch);

                    info_text += fmt::format("Detected Pitch: {:.2f} Hz ({:.1f} cents from {}, MIDI {})\n",
                                             *pitch, GetCentsDifference(closest_musical_note.pitch, *pitch),
                                             closest_musical_note.name, closest_musical_note.midi_note);
                } else {
                    info_text += "Detected Pitch: No pitch could be found\n";
                }
            }

            if (EndsWith(info_text, "\n")) info_text.resize(info_text.size() - 1);
            MessageWithNewLine(GetName(), f, "Info:\n{}", info_text);
        }
    } else {
        auto output_json = nlohmann::json::array();
        if (m_path_as_key) {
            output_json = nlohmann::json::object();
        }

        for (auto &f : files) {
            nlohmann::json file_info;

            if (!f.GetAudio().metadata.IsEmpty()) {
                std::stringstream ss {};
                try {
                    cereal::JSONOutputArchive archive(ss);
                    archive(cereal::make_nvp("Metadata", f.GetAudio().metadata));
                    ss << "}";

                    // Parse the cereal JSON output and add it to our nlohmann::json object
                    auto metadata_json = nlohmann::json::parse(ss.str());
                    if (metadata_json.contains("Metadata")) {
                        file_info["metadata"] = metadata_json["Metadata"];
                    }
                } catch (const std::exception &e) {
                    ErrorWithNewLine(GetName(), f, "Internal error when writing the metadata: {}", e.what());
                    file_info["metadata_error"] = e.what();
                }
            } else {
                file_info["metadata"] = nullptr;
            }

            file_info["channels"] = f.GetAudio().num_channels;
            file_info["sample_rate"] = f.GetAudio().sample_rate;
            file_info["frames"] = f.GetAudio().NumFrames();
            file_info["length_seconds"] = (double)f.GetAudio().NumFrames() / (double)f.GetAudio().sample_rate;
            file_info["bit_depth"] = f.GetAudio().bits_per_sample;

            auto const rms = GetRMS(f.GetAudio().interleaved_samples);
            auto const peak = GetPeak(f.GetAudio().interleaved_samples);
            auto const crest_factor = peak / rms;
            file_info["rms_db"] = AmpToDB(rms);
            file_info["peak_db"] = AmpToDB(peak);
            file_info["crest_factor_db"] = AmpToDB(crest_factor);
            file_info["crest_factor"] = crest_factor;

            if (m_detect_pitch) {
                if (auto const pitch = f.GetAudio().DetectPitch()) {
                    const auto closest_musical_note = FindClosestMidiPitch(*pitch);
                    file_info["detected_pitch_hz"] = *pitch;
                    file_info["detected_pitch_nearest_note"] = closest_musical_note.name;
                    file_info["detected_pitch_nearest_note_midi"] = closest_musical_note.midi_note;
                    file_info["detected_pitch_cents_to_nearest"] =
                        GetCentsDifference(closest_musical_note.pitch, *pitch);
                }
            }

            if (m_field_filter_regex) {
                std::regex filter_regex(*m_field_filter_regex);
                nlohmann::json filtered_file_info;
                for (auto it = file_info.begin(); it != file_info.end(); ++it) {
                    if (std::regex_search(it.key(), filter_regex)) {
                        filtered_file_info[it.key()] = it.value();
                    }
                }
                file_info = filtered_file_info;
            }

            if (m_path_as_key) {
                output_json[f.OriginalPath()] = file_info;
            } else {
                file_info["path"] = f.OriginalPath();
                output_json.push_back(file_info);
            }
        }

        switch (m_format) {
            case Format::Json: fmt::print("{}\n", output_json.dump(2)); break;
            case Format::Lua:
                if (g_signet_invocation_args && g_signet_invocation_args->size()) {
                    fmt::print("-- {} ",
                               fs::path((*g_signet_invocation_args)[0]).filename().generic_string());
                    for (size_t i = 1; i < g_signet_invocation_args->size(); ++i) {
                        fmt::print("{} ", (*g_signet_invocation_args)[i]);
                    }
                    fmt::print("\n");
                }
                fmt::print("return {}\n", JsonToLuaTable(output_json));
                break;
        }
    }
}

TEST_CASE("PrintInfoCommand") {
    auto test_audio = TestHelpers::CreateSingleOscillationSineWave(2, 44100, 44100);

    // No assertions, just check that it runs without crashing

    SUBCASE("Regular output") {
        TestHelpers::ProcessBufferWithCommand<PrintInfoCommand>("print-info", test_audio);
    }

    SUBCASE("JSON output") {
        TestHelpers::ProcessBufferWithCommand<PrintInfoCommand>("print-info --format json", test_audio);
    }

    SUBCASE("Lua output") {
        TestHelpers::ProcessBufferWithCommand<PrintInfoCommand>("print-info --format lua", test_audio);
    }
}
