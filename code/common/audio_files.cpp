#include "audio_files.h"

#include "CLI11.hpp"
#include "doctest.hpp"

#include "backup.h"
#include "common.h"
#include "filepath_set.h"

AudioFiles::AudioFiles(const std::vector<std::string> &path_items, const bool recursive_directory_search) {
    std::string parse_error;
    const auto all_matched_filepaths =
        FilepathSet::CreateFromPatterns(path_items, recursive_directory_search, &parse_error);
    if (!all_matched_filepaths) {
        throw CLI::ValidationError("Input files", parse_error);
    }

    if (all_matched_filepaths->Size() == 0) {
        std::string error {"there are no files that match the pattern "};
        for (const auto &i : path_items) {
            error += i + " ";
        }
        throw CLI::ValidationError("Input files", error);
    }

    ReadAllAudioFiles(*all_matched_filepaths);
}

AudioFiles::AudioFiles(const tcb::span<EditTrackedAudioFile> files) {
    m_all_files.assign(files.begin(), files.end());
    CreateFoldersDataStructure();
}

void AudioFiles::CreateFoldersDataStructure() {
    for (auto &f : m_all_files) {
        fs::path parent = ".";
        if (f.GetPath().has_parent_path()) parent = f.GetPath().parent_path();
        if (m_folders.find(parent) == m_folders.end()) {
            m_folders.insert({parent, {}});
        }
        m_folders[parent].push_back(&f);
    }
}

void AudioFiles::ReadAllAudioFiles(const FilepathSet &paths) {
    for (const auto &path : paths) {
        if (!IsPathReadableAudioFile(path)) continue;
        std::error_code ec;
        const auto proximate = fs::proximate(path);
        if (ec) {
            ErrorWithNewLine("Signet",
                             "Internal error in function {}: fs::proximate failed for path {} for reason {}",
                             __FUNCTION__, path, ec.message());
            m_all_files.push_back(path);
        } else {
            m_all_files.push_back(proximate);
        }
    }
    MessageWithNewLine("Signet", "Found {} matching files", m_all_files.size());
    CreateFoldersDataStructure();
}

bool AudioFiles::WouldWritingAllFilesCreateConflicts() {
    std::set<fs::path> files_set;
    bool file_conflicts = false;
    for (const auto &f : m_all_files) {
        if (files_set.find(f.GetPath()) != files_set.end()) {
            ErrorWithNewLine("Signet", "filepath {} would have the same filename as another file",
                             f.GetPath());
            file_conflicts = true;
        }
        files_set.insert(f.GetPath());
    }
    if (file_conflicts) {
        ErrorWithNewLine("Signet",
                         "files could be unexpectedly overwritten, please review your renaming settings, "
                         "no action will be taken now");
        return true;
    }
    return false;
}

static fs::path PathWithNewExtension(fs::path path, AudioFileFormat format) {
    path.replace_extension(GetLowercaseExtension(format));
    return path;
}

bool AudioFiles::WriteFilesThatHaveBeenEdited(SignetBackup &backup) {
    if (WouldWritingAllFilesCreateConflicts()) {
        return false;
    }

    bool error_occurred = false;
    for (auto &file : m_all_files) {
        const bool file_data_changed = file.AudioChanged();
        const bool file_renamed = file.PathChanged();
        const bool file_format_changed = file.FormatChanged();

        if (file_renamed) {
            if (!file_data_changed && !file_format_changed) {
                // only renamed
                if (!backup.MoveFile(file.OriginalPath(), file.GetPath())) {
                    error_occurred = true;
                    break;
                }
            } else if ((!file_data_changed && file_format_changed) ||
                       (file_data_changed && file_format_changed)) {
                // renamed and new format
                if (!backup.CreateFile(PathWithNewExtension(file.GetPath(), file.GetAudio().format),
                                       file.GetAudio(), true)) {
                    error_occurred = true;
                    break;
                }
                if (!backup.DeleteFile(file.OriginalPath())) {
                    error_occurred = true;
                    break;
                }
            } else if (file_data_changed && !file_format_changed) {
                // renamed and new data
                if (!backup.CreateFile(file.GetPath(), file.GetAudio(), true)) {
                    error_occurred = true;
                    break;
                }
                if (!backup.DeleteFile(file.OriginalPath())) {
                    error_occurred = true;
                    break;
                }
            }
        } else {
            REQUIRE(file.GetPath() == file.OriginalPath());
            if ((file_format_changed && !file_data_changed) || (file_format_changed && file_data_changed)) {
                // only new format
                if (!backup.CreateFile(PathWithNewExtension(file.OriginalPath(), file.GetAudio().format),
                                       file.GetAudio(), false)) {
                    error_occurred = true;
                    break;
                }
                if (!backup.DeleteFile(file.OriginalPath())) {
                    error_occurred = true;
                    break;
                }
            } else if (!file_format_changed && file_data_changed) {
                // only new data
                if (!backup.OverwriteFile(file.OriginalPath(), file.GetAudio())) {
                    error_occurred = true;
                    break;
                }
            }
        }
    }

    if (error_occurred) {
        ErrorWithNewLine(
            "Signet",
            "An error happened while backing-up or writing an audio files. Signet has stopped. Run 'signet undo' to undo any changes that happened up to the point of this error");
    }

    return !error_occurred;
}
