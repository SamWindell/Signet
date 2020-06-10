#pragma once

#include <optional>

#include "subcommand.h"

class Folderiser final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    bool ProcessFilename(fs::path &path, const AudioFile &input) override;
    void Run(SubcommandProcessor &processor) override { processor.ProcessAllFiles(*this); }

  private:
    std::string m_filename_pattern;
    std::string m_out_folder;
};
