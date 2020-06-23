#pragma once

#include <memory>

#include "doctest.hpp"
#include "json.hpp"

#include "audio_file.h"
#include "backup.h"
#include "common.h"
#include "filesystem.hpp"
#include "input_files.h"
#include "subcommand.h"

class SignetInterface final {
  public:
    SignetInterface();

    int Main(const int argc, const char *const argv[]);

  private:
    std::vector<std::unique_ptr<Subcommand>> m_subcommands {};
    SignetBackup m_backup {};

    InputAudioFiles m_input_audio_files {};
    bool m_recursive_directory_search {};
};
