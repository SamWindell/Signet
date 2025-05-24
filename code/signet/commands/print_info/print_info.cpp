#include "print_info.h"

#include "CLI11.hpp"
#include "json.hpp"
#include <cereal/archives/json.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "doctest.hpp"
#include "gain_calculators.h"
#include "test_helpers.h"

CLI::App *PrintInfoCommand::CreateCommandCLI(CLI::App &app) {
    auto printer = app.add_subcommand(
        "print-info",
        "Prints information about the audio file(s), such as the embedded metadata, sample-rate and RMS.");
    printer->add_flag_callback(
        "--json",
        [this]() {
            // We want stdout to be uncluttered by messages so the user can easily parse the JSON output.
            g_messages_enabled = false;
            m_json_output = true;
        },
        "Output the information as JSON array.");
    return printer;
}

void PrintInfoCommand::ProcessFiles(AudioFiles &files) {
    if (m_json_output) {
        auto output_array = nlohmann::json::array();

        for (auto &f : files) {
            nlohmann::json file_info;
            file_info["path"] = f.OriginalPath();

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

            output_array.push_back(file_info);
        }

        // Output the JSON array
        fmt::print("{}\n", output_array.dump(2)); // Pretty print with 2-space indentation
    } else {
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

            if (EndsWith(info_text, "\n")) info_text.resize(info_text.size() - 1);
            MessageWithNewLine(GetName(), f, "Info:\n{}", info_text);
        }
    }
}

TEST_CASE("PrintInfoCommand") {
    auto test_audio = TestHelpers::CreateSingleOscillationSineWave(2, 44100, 44100);

    SUBCASE("Regular output") {
        TestHelpers::ProcessBufferWithCommand<PrintInfoCommand>("print-info", test_audio);
        // No assertions, just check that it runs without crashing
    }

    SUBCASE("JSON output") {
        TestHelpers::ProcessBufferWithCommand<PrintInfoCommand>("print-info --json", test_audio);
        // No assertions, just check that it runs without crashing
    }
}
