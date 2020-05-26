#pragma once

#include <memory>

#include "json.hpp"

#include "common.h"
#include "filesystem.hpp"
#include "subcommand.h"

class Subcommand;

class PatternMatchingFilename {
  public:
    PatternMatchingFilename() {}
    PatternMatchingFilename(std::string n) : str(n) {}
    bool MatchesRaw(const std::string &filepath) const { return PatternMatch(str, filepath); }
    bool IsPattern() const { return str.find('*') != std::string::npos; }
    bool IsSingleFile() const { return !IsPattern() && !ghc::filesystem::is_directory(GetPattern()); }
    bool Matches(const std::string &filepath) const {
        if (MatchesRaw(filepath)) {
            return true;
        }
        if (IsPattern()) {
            return false;
        }
        if (ghc::filesystem::is_directory(GetPattern()) && StartsWith(GetPattern(), filepath)) {
            return true;
        }
        return false;
    }
    std::string GetPattern() const { return str; }
    std::string GetRootDirectory() const {
        const auto slash_pos = str.rfind('/');
        const auto pattern_pos = str.find('*');
        const auto has_slash = slash_pos != std::string::npos;
        const auto has_pattern = pattern_pos != std::string::npos;

        if (has_slash && !has_pattern) {
            return str;
        }

        if (!has_slash && !has_pattern) {
            return ".";
        }

        if (!has_slash && has_pattern) {
            return ".";
        }

        assert(has_pattern && has_slash);
        if (pattern_pos < slash_pos) {
            return ".";
        }
        return str.substr(0, slash_pos);
    }

  private:
    std::string str {};
};

class SignetBackup {
  public:
    SignetBackup() {
        m_backup_dir = "signet-backup";
        if (!ghc::filesystem::is_directory(m_backup_dir)) {
            ghc::filesystem::create_directory(m_backup_dir);
        }

        m_backup_files_dir = m_backup_dir / "files";
        m_database_file = m_backup_dir / "backup.json";
        try {
            std::ifstream i(m_database_file.generic_string(), std::ofstream::in | std::ofstream::binary);
            i >> m_database;
            m_parsed_json = true;
            i.close();
        } catch (const nlohmann::detail::parse_error &e) {
            std::cout << e.what() << "\n";
        } catch (...) {
            std::cout << "other exception\n";
            throw;
        }
    }

    bool LoadBackup() {
        if (!m_parsed_json) return false;
        for (auto [hash, path] : m_database["files"].items()) {
            std::cout << "Loading backed-up file " << path << "\n";
            ghc::filesystem::copy_file(m_backup_files_dir / hash, path,
                                       ghc::filesystem::copy_options::update_existing);
        }
        return true;
    }

    void ResetBackup() {
        ghc::filesystem::remove_all(m_backup_files_dir);
        ghc::filesystem::create_directory(m_backup_files_dir);
        ghc::filesystem::remove(m_database_file);
        m_database = {};
    }

    void AddFileToBackup(const ghc::filesystem::path &path) {
        if (!ghc::filesystem::is_directory(m_backup_files_dir)) {
            ghc::filesystem::create_directory(m_backup_files_dir);
        }

        const auto hash_string = std::to_string(ghc::filesystem::hash_value(path));
        ghc::filesystem::copy_file(path, m_backup_files_dir / hash_string);
        m_database["files"][hash_string] = path.generic_string();

        std::cout << "Backing-up file " << path << "\n";
        std::ofstream o(m_database_file.generic_string(), std::ofstream::out | std::ofstream::binary);
        o << m_database << std::endl;
        o.close();
    }

  private:
    ghc::filesystem::path m_database_file {};
    ghc::filesystem::path m_backup_dir {};
    ghc::filesystem::path m_backup_files_dir {};
    nlohmann::json m_database {};
    bool m_parsed_json {};
};

class SignetInterface {
  public:
    SignetInterface();

    int Main(const int argc, const char *const argv[]);
    void ProcessAllFiles(Subcommand &subcommand);
    bool IsProcessingMultipleFiles() const { return !m_input_filepath_pattern.IsSingleFile(); }

  private:
    void ProcessFile(Subcommand &subcommand,
                     const ghc::filesystem::path input_filepath,
                     ghc::filesystem::path output_filepath);

    std::vector<std::unique_ptr<Subcommand>> m_subcommands {};

    SignetBackup m_backup {};

    bool m_overwrite {};
    std::optional<ghc::filesystem::path> m_output_filepath {};
    PatternMatchingFilename m_input_filepath_pattern {};
};
