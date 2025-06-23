#pragma once

#include "command.h"
#include "json.hpp"

class PrintInfoCommand : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "PrintInfo"; }

  private:
    enum class Format { Text, Json, Lua };
    Format m_format = Format::Text;
    bool m_detect_pitch = false;
    bool m_path_as_key = false;
    std::optional<std::string> m_field_filter_regex {};

    nlohmann::json CalculateFileInfo(EditTrackedAudioFile &file) const;
};
