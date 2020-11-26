#include "input_files.h"

#include "CLI11.hpp"

#include "backup.h"
#include "common.h"

InputAudioFiles::InputAudioFiles(const std::string &pathnames_comma_delimed,
                                 const bool recursive_directory_search) {
    std::string parse_error;
    const auto all_matched_filenames =
        FilePathSet::CreateFromPatterns(pathnames_comma_delimed, recursive_directory_search, &parse_error);
    if (!all_matched_filenames) {
        throw CLI::ValidationError("Input files", parse_error);
    }

    if (all_matched_filenames->Size() == 0) {
        throw CLI::ValidationError("Input files",
                                   "there are no files that match the pattern " + pathnames_comma_delimed);
    }

    m_is_single_file = all_matched_filenames->IsSingleFile();
    ReadAllAudioFiles(*all_matched_filenames);
}

void InputAudioFiles::ReadAllAudioFiles(const FilePathSet &paths) {
    for (const auto &path : paths) {
        if (!IsAudioFileReadable(path)) continue;
        std::error_code ec;
        const auto proximate = fs::proximate(path);
        if (ec) {
            ErrorWithNewLine("Internal error in function " __FUNCTION__ ": fs::proximate failed for path ",
                             path, " for reason ", ec.message());
            m_all_files.push_back(path);
        } else {
            m_all_files.push_back(proximate);
        }
    }
}

bool InputAudioFiles::WouldWritingAllFilesCreateConflicts() {
    std::set<fs::path> files_set;
    bool file_conflicts = false;
    for (const auto &f : GetAllFiles()) {
        if (files_set.find(f.GetPath()) != files_set.end()) {
            ErrorWithNewLine("filepath ", f.GetPath(), " would have the same filename as another file");
            file_conflicts = true;
        }
        files_set.insert(f.GetPath());
    }
    if (file_conflicts) {
        ErrorWithNewLine("files could be unexpectedly overwritten, please review your renaming settings, "
                         "no action will be taken now");
        return true;
    }
    return false;
}

static fs::path PathWithNewExtension(fs::path path, AudioFileFormat format) {
    path.replace_extension(GetLowercaseExtension(format));
    return path;
}

bool InputAudioFiles::WriteAllAudioFiles(SignetBackup &backup) {
    if (WouldWritingAllFilesCreateConflicts()) {
        return false;
    }

    bool error_occurred = false;
    for (auto &file : GetAllFiles()) {
        const bool file_data_changed = file.AudioChanged();
        const bool file_renamed = file.FilepathChanged();
        const bool file_format_changed = file.FormatChanged();

        if (file_renamed) {
            if (!file_data_changed && !file_format_changed) {
                // only renamed
                if (!backup.MoveFile(file.original_path, file.GetPath())) {
                    error_occurred = true;
                    break;
                }
            } else if ((!file_data_changed && file_format_changed) ||
                       (file_data_changed && file_format_changed)) {
                // renamed and new format
                if (!backup.CreateFile(PathWithNewExtension(file.GetPath(), file.GetAudio().format),
                                       file.GetAudio())) {
                    error_occurred = true;
                    break;
                }
                if (!backup.DeleteFile(file.original_path)) {
                    error_occurred = true;
                    break;
                }
            } else if (file_data_changed && !file_format_changed) {
                // renamed and new data
                if (!backup.CreateFile(file.GetPath(), file.GetAudio())) {
                    error_occurred = true;
                    break;
                }
                if (!backup.DeleteFile(file.original_path)) {
                    error_occurred = true;
                    break;
                }
            }
        } else {
            REQUIRE(file.GetPath() == file.original_path);
            if ((file_format_changed && !file_data_changed) || (file_format_changed && file_data_changed)) {
                // only new format
                if (!backup.CreateFile(PathWithNewExtension(file.original_path, file.GetAudio().format),
                                       file.GetAudio())) {
                    error_occurred = true;
                    break;
                }
                if (!backup.DeleteFile(file.original_path)) {
                    error_occurred = true;
                    break;
                }
            } else if (!file_format_changed && file_data_changed) {
                // only new data
                if (!backup.OverwriteFile(file.original_path, file.GetAudio())) {
                    error_occurred = true;
                    break;
                }
            }
        }
    }

    if (error_occurred) {
        ErrorWithNewLine("An error happened while backing-up/writing an audio files so Signet has stopped. "
                         "However, no damage should be done. Run signet --undo to undo any changes that "
                         "happened up to the point of this error");
    }

    return !error_occurred;
}
