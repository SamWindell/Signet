#pragma once

#include <memory>

#include "doctest.hpp"
#include "json.hpp"

#include "audio_file.h"
#include "backup.h"
#include "common.h"
#include "filesystem.hpp"
#include "pathname_expansion.h"
#include "subcommand.h"

class SignetInterface final : public SubcommandProcessor {
  public:
    SignetInterface();

    int Main(const int argc, const char *const argv[]);
    void ProcessAllFiles(Subcommand &subcommand) override;
    bool IsProcessingMultipleFiles() const override { return !m_inputs.IsSingleFile(); }

  private:
    std::vector<std::unique_ptr<Subcommand>> m_subcommands {};

    SignetBackup m_backup {};

    int m_num_files_processed = 0;
    std::optional<ghc::filesystem::path> m_output_filepath {};
    ExpandedPathnames m_inputs {};
};
