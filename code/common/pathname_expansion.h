#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "filesystem.hpp"
#include "span.hpp"

class FilePathSet {
  public:
    // Creates a FilePathSet from a comma separated list of glob patterns, filenames, or directories.
    // Each part can start with a - to signify that the result should exclude anything that matches it.
    // e.g. *.wav,file.flac,-foo*
    static std::optional<FilePathSet> CreateFromPatterns(const std::string_view comma_delimed_parts,
                                                         bool recursive_directory_search,
                                                         std::string *error = nullptr);

    auto Size() const { return m_paths.size(); }

    auto begin() { return m_paths.begin(); }
    auto end() { return m_paths.end(); }
    auto begin() const { return m_paths.begin(); }
    auto end() const { return m_paths.end(); }

    bool IsSingleFile() const {
        return m_num_file_parts == 1 && m_num_directory_parts == 0 && m_num_wildcard_parts == 0;
    }

  private:
    FilePathSet() {}
    void AddNonExcludedPaths(const tcb::span<const fs::path> paths,
                             const std::vector<std::string_view> &exclude_patterns);
    void Add(const fs::path &path) { m_paths.insert(fs::canonical(path).generic_string()); }
    void Add(const std::vector<fs::path> &paths) {
        for (const auto &p : paths) {
            Add(p);
        }
    }

    int m_num_file_parts {};
    int m_num_wildcard_parts {};
    int m_num_directory_parts {};
    std::unordered_set<std::string> m_paths {};
};
