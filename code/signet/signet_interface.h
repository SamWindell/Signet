#pragma once

#include <memory>

#include "common.h"
#include "filesystem.hpp"
#include "subcommand.h"

class Subcommand;

class PatternMatchingFilename {
  public:
    PatternMatchingFilename() {}
    PatternMatchingFilename(std::string n) : str(n) {}
    bool Matches(std::string filepath) const { return PatternMatch(str, filepath); }
    bool IsPattern() const { return str.find('*') != std::string::npos; }
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

class SignetInterface {
  public:
    SignetInterface();

    int Main(const int argc, const char *const argv[]);
    void ProcessAllFiles(Subcommand &subcommand);
    bool IsProcessingMultipleFiles() const {
        return m_input_filepath_pattern.IsPattern() ||
               ghc::filesystem::is_directory(m_input_filepath_pattern.GetPattern());
    }

  private:
    void ProcessFile(Subcommand &subcommand,
                     const ghc::filesystem::path input_filepath,
                     ghc::filesystem::path output_filepath);

    std::vector<std::unique_ptr<Subcommand>> m_subcommands {};
    bool m_delete_input_files = false;
    ghc::filesystem::path m_output_filepath;
    bool m_recursive_directory_search = false;
    PatternMatchingFilename m_input_filepath_pattern {};
};
