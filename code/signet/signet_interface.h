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

class SignetInterface final : public SubcommandProcessor {
  public:
    SignetInterface();

    int Main(const int argc, const char *const argv[]);
    void ProcessAllFiles(Subcommand &subcommand) override;
    bool IsProcessingMultipleFiles() const override { return !m_input_audio_files.IsSingleFile(); }

  private:
    std::vector<std::unique_ptr<Subcommand>> m_subcommands {};
    SignetBackup m_backup {};
    int m_num_files_processed = 0;

    InputAudioFiles m_input_audio_files {};
    std::optional<fs::path> m_output_filepath {};
    bool m_recursive_directory_search {};
};
