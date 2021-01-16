#pragma once

#include <memory>

#include "doctest.hpp"
#include "json.hpp"

#include "audio_file_io.h"
#include "audio_files.h"
#include "backup.h"
#include "common.h"
#include "filesystem.hpp"
#include "subcommand.h"

namespace SignetResult {
enum SignetResultEnum {
    Success = 0,
    NoFilesMatchingInput,
    NoFilesWereProcessed,
    FailedToWriteFiles,
};
}

class SignetInterface final {
  public:
    SignetInterface();

    int Main(const int argc, const char *const argv[]);

  private:
    std::vector<std::unique_ptr<Subcommand>> m_subcommands {};
    SignetBackup m_backup {};

    AudioFiles m_input_audio_files {};
    bool m_recursive_directory_search {};
};
