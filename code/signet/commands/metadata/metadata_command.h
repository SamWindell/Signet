#pragma once

#include "command.h"

class MetadataCommand final : public Command {
  public:
    CLI::App *CreateCommandCLI(CLI::App &app) override;
    void ProcessFiles(AudioFiles &files) override;
    std::string GetName() const override { return "Metadata"; }
    bool IsReadOnly() const override { return m_mode != Mode::Import; }
    bool AllowsOutputFolder() const override { return m_mode == Mode::Import; }
    bool AllowsSingleOutputFile() const override { return m_mode == Mode::Import; }

  private:
    enum class Mode { None, Export, Import };
    Mode m_mode = Mode::None;
    std::string m_import_source {}; // empty or "-" => stdin; otherwise filename
};
