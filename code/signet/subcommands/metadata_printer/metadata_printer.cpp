#include "metadata_printer.h"

#include "CLI11.hpp"
#include <cereal/archives/json.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

CLI::App *MetadataPrinter::CreateSubcommandCLI(CLI::App &app) {
    auto printer = app.add_subcommand(
        "metadata-print", GetName() + ": prints all of the metadata that Signet can process in the file(s).");
    return printer;
}

void MetadataPrinter::ProcessFiles(const tcb::span<EditTrackedAudioFile> files) {
    for (auto &f : files) {
        std::stringstream ss {};
        {
            try {
                cereal::JSONOutputArchive archive(ss);
                archive(cereal::make_nvp("Metadata", f.GetAudio().metadata));
            } catch (const std::exception &e) {
                ErrorWithNewLine(GetName(), "Internal error when writing fetch the metadata: {}", e.what());
            }
        }
        MessageWithNewLine(GetName(), "Metadata for file {}\n{}", f.GetPath(), ss.str());
    }
}
