#include "print_info.h"

#include "CLI11.hpp"
#include <cereal/archives/json.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "subcommands/normalise/gain_calculators.h"

CLI::App *SampleInfoPrinter::CreateSubcommandCLI(CLI::App &app) {
    auto printer = app.add_subcommand(
        "print-info",
        GetName() +
            ": prints information about the audio file(s), such as the embedded metadata, sample-rate and RMS.");
    return printer;
}

void SampleInfoPrinter::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        std::string info_text;
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
            info_text += ss.str() + "\n";
        } else {
            info_text += "Contains no metadata that Signet understands\n";
        }

        info_text += fmt::format("Channels: {}\n", f.GetAudio().num_channels);
        info_text += fmt::format("Sample Rate: {}\n", f.GetAudio().sample_rate);
        info_text += fmt::format("Bit-depth: {}\n", f.GetAudio().bits_per_sample);
        info_text += fmt::format("RMS: {:.5f}\n", GetRMS(f.GetAudio().interleaved_samples));

        if (EndsWith(info_text, "\n")) info_text.resize(info_text.size() - 1);
        MessageWithNewLine(GetName(), "Info for file {}\n{}", f.GetPath(), info_text);
    }
}
