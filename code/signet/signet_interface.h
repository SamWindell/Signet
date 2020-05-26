#pragma once

#include "common.h"
#include "subcommand_processor.h"

class SignetInterface {
  public:
    SignetInterface();

    int Main(const int argc, const char *argv[]);
    void ProcessAllFiles(Processor &processor);
    bool IsProcessingMultipleFiles() const { return ghc::filesystem::is_directory(m_input_filepath); }

  private:
    void ProcessFile(Processor &processor,
                     const ghc::filesystem::path input_filepath,
                     ghc::filesystem::path output_filepath);

    std::vector<Processor *> m_processors;
    bool m_delete_input_files = false;
    ghc::filesystem::path m_input_filepath;
    ghc::filesystem::path m_output_filepath;
    bool m_recursive_directory_search = false;
};
