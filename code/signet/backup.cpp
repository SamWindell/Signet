#include "backup.h"

#include <iomanip>
#include <iostream>

#include "doctest.hpp"

#include "common.h"
#include "test_helpers.h"
#include "tests_config.h"

SignetBackup::SignetBackup() {
    m_backup_dir = "signet-backup";
    if (!fs::is_directory(m_backup_dir)) {
        fs::create_directory(m_backup_dir);
    }

    m_backup_files_dir = m_backup_dir / "files";
    m_database_file = m_backup_dir / "backup.json";
    if (fs::is_regular_file(m_database_file)) {
        try {
            std::ifstream i(m_database_file.generic_string(), std::ofstream::in | std::ofstream::binary);
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
    if (!m_database["files"].size()) {
        WarningWithNewLine("Signet", "There are no files to backup");
        return false;
    }
    for (auto [hash, path] : m_database["files"].items()) {
        MessageWithNewLine("Signet", "Loading backed-up file ", path);
        try {
            fs::copy_file(m_backup_files_dir / hash, path, fs::copy_options::overwrite_existing);
        } catch (const fs::filesystem_error &e) {
            ErrorWithNewLine("could not copy file from ", e.path1(), " to ", e.path2(),
                             " for reason: ", e.what());
        }
    }
    return true;
}

void SignetBackup::ResetBackup() {
    if (fs::is_directory(m_backup_files_dir)) {
        fs::remove_all(m_backup_files_dir);
        fs::create_directory(m_backup_files_dir);
    }
    if (fs::is_regular_file(m_database_file)) {
        fs::remove(m_database_file);
    }
    m_database = {};
}

void SignetBackup::AddFileToBackup(const fs::path &path) {
    if (!fs::is_directory(m_backup_files_dir)) {
        fs::create_directory(m_backup_files_dir);
    }

    const auto hash_string = std::to_string(fs::hash_value(path));
    fs::copy_file(path, m_backup_files_dir / hash_string, fs::copy_options::update_existing);
    m_database["files"][hash_string] = path.generic_string();

    MessageWithNewLine("Signet", "Backing-up file ", path);
    std::ofstream o(m_database_file.generic_string(), std::ofstream::out | std::ofstream::binary);
    o << std::setw(2) << m_database << std::endl;
    o.close();
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
        b.ResetBackup();
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
