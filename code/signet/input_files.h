#pragma once

#include "CLI11.hpp"

#include "audio_file.h"
#include "pathname_expansion.h"
#include "string_utils.h"
#include "types.h"

struct InputAudioFile {
    InputAudioFile(const AudioFile &_file, fs::path _path) {
        file = _file;
        path = _path;
        new_filename = GetJustFilenameWithNoExtension(path);
    }

    AudioFile file {};
    fs::path path {};
    std::string new_filename {};
    bool renamed {};
    bool file_edited {};
};

class InputAudioFiles {
  public:
    InputAudioFiles() {}
    InputAudioFiles(const std::string &pathnames_comma_delimed, const bool recursive_directory_search) {
        std::string parse_error;
        const auto all_matched_filenames = FilePathSet::CreateFromPatterns(
            pathnames_comma_delimed, recursive_directory_search, &parse_error);
        if (!all_matched_filenames) {
            throw CLI::ValidationError("Input files", parse_error);
        }

        if (all_matched_filenames->Size() == 0) {
            throw CLI::ValidationError("Input files", "there are no files that match the pattern " +
                                                          pathnames_comma_delimed);
        }

        m_is_single_file = all_matched_filenames->IsSingleFile();
        for (const auto &path : *all_matched_filenames) {
            if (!IsAudioFileReadable(path)) continue;
            if (auto file = ReadAudioFile(path)) {
                m_all_files.push_back({*file, path});
            }
        }
    }

    bool IsSingleFile() const { return m_is_single_file; }
    std::vector<InputAudioFile> &GetAllFiles() { return m_all_files; }
    usize NumFiles() const { return m_all_files.size(); }

  private:
    bool m_is_single_file {};
    std::vector<InputAudioFile> m_all_files {};
};
