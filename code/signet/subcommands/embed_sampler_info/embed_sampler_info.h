#pragma once

#include "subcommand.h"

class EmbedSamplerInfo : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;

    std::string GetName() const override { return "Sample Info Embeder"; }

  private:
    std::optional<int> m_root_number;
    std::optional<std::string> m_root_regex_pattern;
    std::optional<std::string> m_root_auto_detect_name;

    bool m_note_range_auto_map {};
    std::optional<int> m_low_note_number;
    std::optional<std::string> m_low_note_regex_pattern;
    std::optional<int> m_high_note_number;
    std::optional<std::string> m_high_note_regex_pattern;

    std::optional<int> m_low_velo_number;
    std::optional<std::string> m_low_velo_regex_pattern;
    std::optional<int> m_high_velo_number;
    std::optional<std::string> m_high_velo_regex_pattern;
};
