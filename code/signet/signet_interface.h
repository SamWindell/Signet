#pragma once

#include <memory>

#include "common.h"
#include "filesystem.hpp"
#include "subcommand.h"

class Subcommand;

class SignetInterface {
  public:
    SignetInterface();

    int Main(const int argc, const char *const argv[]);
    void ProcessAllFiles(Subcommand &subcommand);
    bool IsProcessingMultipleFiles() const { return ghc::filesystem::is_directory(m_input_filepath); }

  private:
    void ProcessFile(Subcommand &subcommand,
                     const ghc::filesystem::path input_filepath,
                     ghc::filesystem::path output_filepath);

    std::vector<std::unique_ptr<Subcommand>> m_subcommands;
    bool m_delete_input_files = false;
    ghc::filesystem::path m_input_filepath;
    ghc::filesystem::path m_output_filepath;
    bool m_recursive_directory_search = false;
};
