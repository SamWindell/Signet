#pragma once

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "filesystem.hpp"
#include "span.hpp"

class FilePathSet {
  public:
    // Creates a FilePathSet from a vector of glob patterns, filenames, or directories.
    // Each part can start with a - to signify that the result should exclude anything that matches it.
    // e.g. ["*.wav", "file.flac", "-foo*"]
    static std::optional<FilePathSet> CreateFromPatterns(const std::vector<std::string> &parts,
                                                         bool recursive_directory_search,
                                                         std::string *error = nullptr);

    auto Size() const { return m_paths.size(); }

    auto begin() { return m_paths.begin(); }
    auto end() { return m_paths.end(); }
    auto begin() const { return m_paths.begin(); }
    auto end() const { return m_paths.end(); }

  private:
    FilePathSet() {}
    void AddNonExcludedPaths(const tcb::span<const fs::path> paths,
                             const std::vector<std::string> &exclude_patterns);
    void Add(const fs::path &path) { m_paths.insert(fs::canonical(path)); }
    void Add(const std::vector<fs::path> &paths) {
        for (const auto &p : paths) {
            Add(p);
        }
    }

    int m_num_file_parts {};
    int m_num_wildcard_parts {};
    int m_num_directory_parts {};
    std::set<fs::path> m_paths {};
};

void ForEachFileInDirectory(const std::string_view directory,
                            const bool recursively,
                            const std::function<void(const fs::path &)> callback);
