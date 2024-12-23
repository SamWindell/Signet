#pragma once

#include <memory>

#include "doctest.hpp"
#include "json.hpp"

#include "audio_file_io.h"
#include "audio_files.h"
#include "backup.h"
#include "command.h"
#include "common.h"
#include "filesystem.hpp"

namespace SignetResult {
enum SignetResultEnum {
    Success = 0,
    NoFilesMatchingInput,
    NoFilesWereProcessed,
    FailedToWriteFiles,
    FatalErrorOcurred,
    WarningsAreErrors,
};
}

class SignetInterface final {
  public:
    SignetInterface();

    int Main(const int argc, const char *const argv[]);

  private:
    std::vector<std::unique_ptr<Command>> m_commands {};
    SignetBackup m_backup {};

    AudioFiles m_input_audio_files {};
    bool m_recursive_directory_search {};
    fs::path m_make_docs_filepath {};
    std::optional<fs::path> m_output_path {};
    std::optional<fs::path> m_single_output_file {};
};
