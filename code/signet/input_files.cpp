#include "input_files.h"

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
        if (auto file = ReadAudioFile(path)) {
            m_all_files.push_back({*file, path});
        }
    }
}

bool InputAudioFiles::WouldWritingAllFilesCreateConflicts() {
    std::unordered_set<std::string> files_set;
    bool file_conflicts = false;
    for (const auto &f : GetAllFiles()) {
        const auto generic = f.path.generic_string();
        if (files_set.find(generic) != files_set.end()) {
            ErrorWithNewLine("filepath ", generic, " would have the same filename as another file");
            file_conflicts = true;
        }
        files_set.insert(generic);
    }
    if (file_conflicts) {
        ErrorWithNewLine("files could be unexpectedly overwritten, please review your renaming settings, "
                         "no action will be taken now");
        return true;
    }
    return false;
}

static void CreateParentDirectories(const fs::path &path) {
    if (path.has_parent_path()) {
        const auto &parent = path.parent_path();
        if (!fs::is_directory(parent)) {
            try {
                fs::create_directories(parent);
            } catch (const fs::filesystem_error &e) {
                ErrorWithNewLine("failed to create directory ", e.path1(), " for reason: ", e.what());
            }
        }
    }
}

static void MoveFile(const fs::path &from, const fs::path &to) {
    MessageWithNewLine("Signet", "Moving file from ", from, " to ", to);
    CreateParentDirectories(to);
    try {
        fs::rename(from, to);
    } catch (const fs::filesystem_error &e) {
        ErrorWithNewLine("failed to rename ", e.path1(), " to ", e.path2(), " for reason: ", e.what());
    }
}

static void DeleteFile(const fs::path &path) {
    MessageWithNewLine("Signet", "Deleting file ", path);
    try {
        fs::remove(path);
    } catch (const fs::filesystem_error &e) {
        ErrorWithNewLine("failed to remove file ", path, " for reason: ", e.what());
    }
}

static fs::path PathWithNewExtension(fs::path path, AudioFileFormat format) {
    path.replace_extension(GetLowercaseExtension(format));
    return path;
}

static void WriteFile(const fs::path &path, const AudioFile &file) {
    if (!WriteAudioFile(path, file)) {
        ErrorWithNewLine("could not write the wave file ", path);
    }
}

bool InputAudioFiles::WriteAllAudioFiles(SignetBackup &backup) {
    if (WouldWritingAllFilesCreateConflicts()) {
        return false;
    }

    for (const auto &file : GetAllFiles()) {
        const bool file_data_changed = file.file_edited;
        const bool file_renamed = file.renamed;
        const bool file_format_changed = file.file.format != file.original_file_format;

        if (file_renamed) {
            if (!file_data_changed && !file_format_changed) {
                // only renamed
                backup.AddMovedFileToBackup(file.original_path, file.path);
                MoveFile(file.original_path, file.path);
            } else if ((!file_data_changed && file_format_changed) ||
                       (file_data_changed && file_format_changed)) {
                // renamed and new format
                auto new_file = PathWithNewExtension(file.path, file.file.format);
                backup.AddNewlyCreatedFileToBackup(new_file);
                WriteFile(new_file, file.file);

                backup.AddFileToBackup(file.original_path);
                DeleteFile(file.original_path);
            } else if (file_data_changed && !file_format_changed) {
                // renamed and new data
                WriteFile(file.path, file.file);

                backup.AddFileToBackup(file.original_path);
                DeleteFile(file.original_path);
            }
        } else {
            REQUIRE(file.path == file.original_path);
            if ((file_format_changed && !file_data_changed) || (file_format_changed && file_data_changed)) {
                // only new format
                auto new_file = PathWithNewExtension(file.original_path, file.file.format);
                backup.AddNewlyCreatedFileToBackup(new_file);
                WriteFile(new_file, file.file);

                backup.AddFileToBackup(file.original_path);
                DeleteFile(file.original_path);
            } else if (!file_format_changed && file_data_changed) {
                // only new data
                backup.AddFileToBackup(file.original_path);
                WriteFile(file.original_path, file.file);
            }
        }
    }
    return true;
}
