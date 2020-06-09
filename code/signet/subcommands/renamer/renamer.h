#pragma once

#include <optional>

#include "subcommand.h"

class Renamer final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    bool ProcessFilename(fs::path &path, const AudioFile &input) override;
    void Run(SubcommandProcessor &processor) override { processor.ProcessAllFiles(*this); }

  private:
    std::optional<std::string> m_prefix;
    std::optional<std::string> m_suffix;
    std::optional<std::string> m_regex_pattern;
    std::string m_regex_replacement;

    int m_counter {};
};
