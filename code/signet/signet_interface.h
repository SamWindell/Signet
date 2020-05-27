#pragma once

#include <memory>

#include "doctest.hpp"
#include "json.hpp"

#include "backup.h"
#include "common.h"
#include "filesystem.hpp"
#include "pathname_expansion.h"
#include "subcommand.h"

class Subcommand;

class SignetInterface {
  public:
    SignetInterface();

    int Main(const int argc, const char *const argv[]);
    void ProcessAllFiles(Subcommand &subcommand);
    bool IsProcessingMultipleFiles() const { return !m_input_filepath_pattern.IsSingleFile(); }

  private:
    void ProcessFile(Subcommand &subcommand,
                     const ghc::filesystem::path &input_filepath,
                     std::optional<ghc::filesystem::path> output_filepath);

    std::vector<std::unique_ptr<Subcommand>> m_subcommands {};

    SignetBackup m_backup {};

    int m_num_files_processed = 0;
    std::optional<ghc::filesystem::path> m_output_filepath {};
    ExpandedPathnames<> m_input_filepath_pattern {};
};
