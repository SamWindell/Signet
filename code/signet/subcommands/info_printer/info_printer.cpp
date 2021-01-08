#include "info_printer.h"

#include "CLI11.hpp"
#include <cereal/archives/json.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "subcommands/normaliser/gain_calculators.h"

CLI::App *SampleInfoPrinter::CreateSubcommandCLI(CLI::App &app) {
    auto printer = app.add_subcommand(
        "print-info",
        GetName() +
            ": prints information about the audio file(s), such as the embedded metadata, sample-rate and RMS.");
    return printer;
}

void SampleInfoPrinter::ProcessFiles(const tcb::span<EditTrackedAudioFile> files) {
    for (auto &f : files) {
        std::string result;
        if (!f.GetAudio().metadata.IsEmpty()) {
            std::stringstream ss {};
            {
                try {
                    cereal::JSONOutputArchive archive(ss);
                    archive(cereal::make_nvp("Metadata", f.GetAudio().metadata));
                } catch (const std::exception &e) {
                    ErrorWithNewLine(GetName(), "Internal error when writing fetch the metadata: {}",
                                     e.what());
                }
            }
            result += ss.str() + "\n";
        } else {
            result += "Contains no metadata that Signet understands\n";
        }

        result += fmt::format("Channels: {}\n", f.GetAudio().num_channels);
        result += fmt::format("Sample Rate: {}\n", f.GetAudio().sample_rate);
        result += fmt::format("Bit-depth: {}\n", f.GetAudio().bits_per_sample);
        result += fmt::format("RMS: {:.5f}\n", GetRMS(f.GetAudio().interleaved_samples));

        if (EndsWith(result, "\n")) result.resize(result.size() - 1);
        MessageWithNewLine(GetName(), "Info for file {}\n{}", f.GetPath(), result);
    }
}
