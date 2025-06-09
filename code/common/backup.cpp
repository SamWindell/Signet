#include "backup.h"

#include <iomanip>
#include <iostream>
#include <random>

#include "doctest.hpp"

#include "audio_file_io.h"
#include "common.h"
#include "test_helpers.h"
#include "tests_config.h"

static fs::path GetTempDir() {
    std::error_code ec;
    const auto temp_dir = fs::temp_directory_path(ec);
    if (ec) {
        WarningWithNewLine(
            "Backup", {},
            "Could not get the temporary file folder from the OS for reason: {}. Reverting to using the current working directory.",
            ec.message());
        return "."; // cwd
    }
    return temp_dir;
}

static bool CreateDirectoryChecked(const fs::path &dir) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        ErrorWithNewLine("Signet", {}, "Failed to create directory {} for reason {}", dir, ec.message());
        return false;
    }
    return true;
}

static std::string RandomAlphanum() {
    std::random_device rd;
    std::mt19937 gen(rd());

    const auto alphanum = "0123456789abcdefghijklmnopqrstuvwxyz";

    std::uniform_int_distribution<> dis(0, strlen(alphanum) - 1);
    std::string result(10, 'a');
    for (auto &c : result) {
        c = alphanum[dis(gen)];
    }
    return result;
}

SignetBackup::SignetBackup()
    : m_backup_dir(GetTempDir() / "signet-backup")
    , m_backup_files_dir(m_backup_dir / "files")
    , m_database_file(m_backup_dir / "backup.json") {}

bool SignetBackup::LoadBackup() const {
    // We consume the database file in an atomic way by renaming it to a unique temporary file
    const auto temp_database_file = m_database_file.string() + "." + RandomAlphanum() + ".tmp";

    std::error_code ec;
    fs::rename(m_database_file, temp_database_file, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) {
        WarningWithNewLine("Backup", {}, "Could not read json backup file: {}", ec.message());
        return false;
    }

    nlohmann::json database {};
    try {
        std::ifstream i(temp_database_file, std::ifstream::in | std::ifstream::binary);
        i >> database;
    } catch (...) {
        WarningWithNewLine("Backup", {}, "The backup files could not be read");
        return false;
    }

    fs::remove(temp_database_file, ec); // Error is ignored

    if (!database["files"].size() && !database["file_moves"].size() && !database["files_created"].size()) {
        WarningWithNewLine("Backup", {}, "There is no backed-up data");
        return false;
    }

    // Process file deletions
    for (const std::string f : database["files_created"]) {
        MessageWithNewLine("Backup", {}, "Deleting file {} created by Signet", f);
        std::error_code ec;
        fs::remove(f, ec);
        if (ec) {
            ErrorWithNewLine("Backup", {}, "Could not remove file {} for reason: {}", f, ec.message());
        }
    }

    // Process file moves
    for (const auto &[from, to] : database["file_moves"].items()) {
        MessageWithNewLine("Backup", {}, "Restoring moved file to {}", from);
        std::error_code ec;
        fs::rename(to.get<std::string>(), from, ec);
        if (ec) {
            ErrorWithNewLine("Backup", {}, "Could not move file from {} to {} for reason: {}",
                             to.get<std::string>(), from, ec.message());
        }
    }

    // Process file restores
    for (const auto &[hash, path] : database["files"].items()) {
        MessageWithNewLine("Backup", {}, "Loading backed-up file {}", path);
        std::error_code ec;
        fs::copy_file(m_backup_files_dir / hash, path.get<std::string>(),
                      fs::copy_options::overwrite_existing, ec);
        if (ec) {
            ErrorWithNewLine("Backup", {}, "Could not copy file from {} to {} for reason: {}",
                             (m_backup_files_dir / hash).string(), path.get<std::string>(), ec.message());
        }
    }

    return true;
}

void SignetBackup::ClearBackup() {
    std::error_code ec;

    fs::remove_all(m_backup_files_dir, ec); // Error is ignored
    CreateDirectoryChecked(m_backup_files_dir);

    fs::remove(m_database_file, ec); // Error is ignored

    m_database = {};
}

bool SignetBackup::WriteDatabaseFile() {
    CreateDirectoryChecked(m_backup_dir);

    const auto temp_file = m_database_file.string() + "." + RandomAlphanum() + ".tmp";

    {
        std::ofstream o(temp_file, std::ofstream::out | std::ofstream::binary);
        if (!o) {
            ErrorWithNewLine("Signet", {}, "Could not write to temporary backup database file {}", temp_file);
            return false;
        }

        o << std::setw(2) << m_database << std::endl;
        o.close();

        if (o.fail()) {
            ErrorWithNewLine("Signet", {}, "Failed to write backup database content to {}", temp_file);
            std::error_code ec;
            fs::remove(temp_file, ec); // Clean up failed temp file
            return false;
        }
    }

    // Atomic rename
    std::error_code ec;
    fs::rename(temp_file, m_database_file, ec);
    if (ec) {
        ErrorWithNewLine("Signet", {}, "Could not atomically move {} to {} for reason: {}", temp_file,
                         m_database_file, ec.message());
        fs::remove(temp_file, ec); // Clean up temp file
        return false;
    }

    return true;
}

bool SignetBackup::AddFileToBackup(const fs::path &path) {
    if (!CreateDirectoryChecked(m_backup_files_dir)) return false;

    const auto hash_string = std::to_string(fs::hash_value(path));
    const auto backup_path = m_backup_files_dir / hash_string;

    std::error_code ec;
    fs::copy_file(path, backup_path, fs::copy_options::update_existing, ec);
    if (ec) {
        ErrorWithNewLine("Signet", {},
                         "Backing up file failed. Could not copy file from {} to {} for reason: {}", path,
                         backup_path, ec.message());
        return false;
    }

    m_database["files"][hash_string] = path.generic_string();
    return WriteDatabaseFile();
}

static bool CreateParentDirectories(const fs::path &path) {
    if (path.has_parent_path()) {
        const auto &parent = path.parent_path();
        return CreateDirectoryChecked(parent);
    }
    return true;
}

bool SignetBackup::DeleteFile(const fs::path &path) {
    ClearOldBackIfNeeded();
    if (!AddFileToBackup(path)) return false;

    MessageWithNewLine("Signet", path, "Deleting file");
    std::error_code ec;
    fs::remove(path, ec);
    if (ec) {
        ErrorWithNewLine("Signet", path, "Failed to remove file for reason: {}", ec.message());
        return false;
    }
    return true;
}

static bool CheckForValidPath(const fs::path &path) {
    std::string error;
    if (!IsPathSyntacticallyCorrect(path.generic_string(), &error)) {
        ErrorWithNewLine("Signet", path, "{}", error);
        return false;
    }
    return true;
}

bool SignetBackup::MoveFile(const fs::path &from, const fs::path &to) {
    ClearOldBackIfNeeded();
    MessageWithNewLine("Signet", {}, "Moving file from {} to {}", from, to);

    if (!CheckForValidPath(from) || !CheckForValidPath(to)) return false;
    if (!CreateParentDirectories(to)) return false;

    if (from == to) {
        MessageWithNewLine("Signet", {}, "Source and destination paths are the same, no action taken");
        return true;
    }

    std::error_code ec;
    if (fs::exists(to, ec)) {
        ErrorWithNewLine("Signet", {}, "Destination file {} already exists", to);
        return false;
    }

    fs::rename(from, to, ec);
    if (ec) {
        ErrorWithNewLine("Signet", {}, "Moving file failed for reason: {}", ec.message());
        return false;
    }

    if (!AddMovedFileToBackup(from, to)) {
        // If backup fails, try to undo the move
        fs::rename(to, from, ec);
        if (ec) {
            ErrorWithNewLine("Signet", {}, "Failed to undo move after backup failure: {}", ec.message());
        }
        return false;
    }

    return true;
}

void SignetBackup::ClearOldBackIfNeeded() {
    if (!m_old_backup_cleared) {
        MessageWithNewLine("Signet", {}, "Clearing the old backup data ready for new changes to be saved");
        ClearBackup();
        m_old_backup_cleared = true;
    }
}

bool SignetBackup::AddNewlyCreatedFileToBackup(const fs::path &path) {
    if (!CreateDirectoryChecked(m_backup_files_dir)) return false;

    m_database["files_created"].push_back(path.generic_string());
    return WriteDatabaseFile();
}

bool SignetBackup::AddMovedFileToBackup(const fs::path &from, const fs::path &to) {
    if (!CreateDirectoryChecked(m_backup_files_dir)) return false;

    m_database["file_moves"][from.generic_string()] = to.generic_string();
    return WriteDatabaseFile();
}

static bool WriteFile(const fs::path &path, const AudioData &data) {
    if (!WriteAudioFile(path, data)) {
        ErrorWithNewLine("Signet", path, "Could not write the file");
        return false;
    }
    return true;
}

bool SignetBackup::CreateFile(const fs::path &path, const AudioData &data, bool create_directories) {
    ClearOldBackIfNeeded();
    if (!CheckForValidPath(path)) return false;

    if (create_directories) {
        if (!CreateParentDirectories(path)) return false;
    }

    std::error_code ec;
    if (fs::exists(path, ec)) {
        return OverwriteFile(path, data);
    }

    MessageWithNewLine("Signet", path, "Creating file");
    if (!WriteFile(path, data)) return false;
    return AddNewlyCreatedFileToBackup(path);
}

bool SignetBackup::OverwriteFile(const fs::path &path, const AudioData &data) {
    ClearOldBackIfNeeded();
    if (!AddFileToBackup(path)) return false;
    MessageWithNewLine("Signet", path, "Overwriting file");
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
