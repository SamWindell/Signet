#include "backup.h"

#include <iomanip>
#include <iostream>

#include "doctest.hpp"

#include "audio_file.h"
#include "common.h"
#include "test_helpers.h"
#include "tests_config.h"

static fs::path GetTempDir() {
    try {
        const auto temp_dir = fs::temp_directory_path();
        return temp_dir;
    } catch (const fs::filesystem_error &e) {
        WarningWithNewLine("could not get the temporary file folder from the OS for reason: ", e.what(),
                           ". Reverting to using the current working directory.");
    }
    return "."; // cwd
}

static bool CreateDirectoryChecked(const fs::path &dir) {
    try {
        fs::create_directories(dir);
    } catch (const fs::filesystem_error &e) {
        ErrorWithNewLine("failed to create directory ", e.path1(), " for reason: ", e.what());
        return false;
    }
    return true;
}

static bool CreateDirectoriesChecked(const fs::path &dir) {
    try {
        MessageWithNewLine("Signet", "Creating directories ", dir);
        fs::create_directories(dir);
    } catch (const fs::filesystem_error &e) {
        ErrorWithNewLine("failed to create directory ", e.path1(), " for reason: ", e.what());
        return false;
    }
    return true;
}

SignetBackup::SignetBackup() {
    m_backup_dir = GetTempDir() / "signet-backup";
    if (!fs::is_directory(m_backup_dir)) {
        CreateDirectoryChecked(m_backup_dir);
    }

    m_backup_files_dir = m_backup_dir / "files";
    m_database_file = m_backup_dir / "backup.json";
    if (fs::is_regular_file(m_database_file)) {
        try {
            std::ifstream i(m_database_file.generic_string(), std::ofstream::in | std::ofstream::binary);
            if (!i) {
                ErrorWithNewLine("could not open backup database file ", m_database_file);
                return;
            }
            i >> m_database;
            m_parsed_json = true;
            i.close();
        } catch (const nlohmann::detail::parse_error &e) {
            WarningWithNewLine("could not parse json backup file ", m_database_file, e.what());
        }
    }
}

bool SignetBackup::LoadBackup() {
    if (!m_parsed_json) {
        WarningWithNewLine("Signet", "The backup files could not be read");
        return false;
    }
    if (!m_database["files"].size() && !m_database["file_moves"].size() &&
        !m_database["files_created"].size()) {
        WarningWithNewLine("Signet", "There is no backed-up data");
        return false;
    }

    for (const std::string &f : m_database["files_created"]) {
        MessageWithNewLine("Signet", "Deleting file created by Signet ", f);
        try {
            fs::remove(f);
        } catch (const fs::filesystem_error &e) {
            ErrorWithNewLine("could not remove file from ", f, " for reason: ", e.what());
        }
    }

    for (const auto &[from, to] : m_database["file_moves"].items()) {
        MessageWithNewLine("Signet", "Restoring moved file to ", from);
        try {
            fs::rename(to.get<std::string>(), from);
        } catch (const fs::filesystem_error &e) {
            ErrorWithNewLine("could not move file from ", e.path1(), " to ", e.path2(),
                             " for reason: ", e.what());
        }
    }

    for (const auto &[hash, path] : m_database["files"].items()) {
        MessageWithNewLine("Signet", "Loading backed-up file ", path);
        try {
            fs::copy_file(m_backup_files_dir / hash, path.get<std::string>(),
                          fs::copy_options::overwrite_existing);
        } catch (const fs::filesystem_error &e) {
            ErrorWithNewLine("could not copy file from ", e.path1(), " to ", e.path2(),
                             " for reason: ", e.what());
        }
    }
    return true;
}

void SignetBackup::ClearBackup() {
    if (fs::is_directory(m_backup_files_dir)) {
        fs::remove_all(m_backup_files_dir);
        CreateDirectoryChecked(m_backup_files_dir);
    }
    if (fs::is_regular_file(m_database_file)) {
        fs::remove(m_database_file);
    }
    m_database = {};
}

bool SignetBackup::WriteDatabaseFile() {
    std::ofstream o(m_database_file.generic_string(), std::ofstream::out | std::ofstream::binary);
    if (!o) {
        ErrorWithNewLine("could not write to backup database file ", m_database_file);
        return false;
    }
    o << std::setw(2) << m_database << std::endl;
    o.close();
    return true;
}

bool SignetBackup::CreateBackupFilesDirIfNeeded() {
    if (!fs::is_directory(m_backup_files_dir)) {
        return CreateDirectoryChecked(m_backup_files_dir);
    }
    return true;
}

bool SignetBackup::AddNewlyCreatedFileToBackup(const fs::path &path) {
    if (!CreateBackupFilesDirIfNeeded()) return false;

    m_database["files_created"].push_back(path.generic_string());
    return WriteDatabaseFile();
}

bool SignetBackup::AddMovedFileToBackup(const fs::path &from, const fs::path &to) {
    if (!CreateBackupFilesDirIfNeeded()) return false;

    MessageWithNewLine("Signet", "Backing-up file move from ", from, " to ", to);
    m_database["file_moves"][from.generic_string()] = to.generic_string();
    return WriteDatabaseFile();
}

bool SignetBackup::AddFileToBackup(const fs::path &path) {
    if (!CreateBackupFilesDirIfNeeded()) return false;

    MessageWithNewLine("Signet", "Backing-up file ", path);
    const auto hash_string = std::to_string(fs::hash_value(path));
    try {
        fs::copy_file(path, m_backup_files_dir / hash_string, fs::copy_options::update_existing);
    } catch (const fs::filesystem_error &e) {
        ErrorWithNewLine("backing up file failed! Could not copy file from ", e.path1(), " to ", e.path2(),
                         " for reason: ", e.what());
        return false;
    }
    m_database["files"][hash_string] = path.generic_string();
    return WriteDatabaseFile();
}

static bool CreateParentDirectories(const fs::path &path) {
    if (path.has_parent_path()) {
        const auto &parent = path.parent_path();
        if (!CreateDirectoriesChecked(parent)) return false;
    }
    return true;
}

bool SignetBackup::DeleteFile(const fs::path &path) {
    if (!AddFileToBackup(path)) return false;
    MessageWithNewLine("Signet", "Deleting file ", path);
    try {
        fs::remove(path);
    } catch (const fs::filesystem_error &e) {
        ErrorWithNewLine("failed to remove file ", path, " for reason: ", e.what());
        return false;
    }
    return true;
}

bool SignetBackup::MoveFile(const fs::path &from, const fs::path &to) {
    MessageWithNewLine("Signet", "Moving file from ", from, " to ", to);
    if (!CreateParentDirectories(to)) return false;
    std::error_code ec;
    assert(!fs::exists(to));
    fs::rename(from, to, ec);
    if (ec) {
        ErrorWithNewLine("Moving file failed for reason: ", ec.message());
        return false;
    }
    if (!AddMovedFileToBackup(from, to)) return false;
    return true;
}

static bool WriteFile(const fs::path &path, const AudioData &data) {
    if (!WriteAudioFile(path, data)) {
        ErrorWithNewLine("could not write the file ", path);
        return false;
    }
    return true;
}

bool SignetBackup::CreateFile(const fs::path &path, const AudioData &data, bool create_directories) {
    MessageWithNewLine("Signet", "Creating file ", path);
    if (create_directories) {
        if (!CreateParentDirectories(path)) return false;
    }
    if (!WriteFile(path, data)) return false;
    return AddNewlyCreatedFileToBackup(path);
}

bool SignetBackup::OverwriteFile(const fs::path &path, const AudioData &data) {
    if (!AddFileToBackup(path)) return false;
    MessageWithNewLine("Signet", "Overwriting file ", path);
    return WriteFile(path, data);
}

TEST_CASE("[SignetBackup]") {
    const std::string filename = "backup_file.wav";

    // create a sine wave file
    {
        const auto buf = TestHelpers::CreateSineWaveAtFrequency(1, 44100, 0.25, 440);
        REQUIRE(WriteAudioFile(filename, buf));
    }

    // back it up
    {
        SignetBackup b;
        b.ClearBackup();
        b.AddFileToBackup(filename);
    }

    // process that same file
    {
        auto file_data = ReadAudioFile(filename);
        REQUIRE(file_data);
        for (auto &s : file_data->interleaved_samples) {
            s = 0;
        }
        REQUIRE(WriteAudioFile(filename, *file_data));
    }

    // load the backup
    {
        SignetBackup b;
        REQUIRE(b.LoadBackup());
    }

    // assert that the backed up version is not processed
    {
        auto file_data = ReadAudioFile(filename);
        REQUIRE(file_data);
        bool silent = true;
        for (auto &s : file_data->interleaved_samples) {
            if (s != 0) {
                silent = false;
                break;
            }
        }
        REQUIRE(!silent);
    }
}
