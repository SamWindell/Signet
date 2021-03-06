#pragma once

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "filesystem.hpp"
#include "span.hpp"

class FilepathSet {
  public:
    // Creates a FilepathSet from a vector of glob patterns, filenames, or directories.
    // Each part can start with a - to signify that the result should exclude anything that matches it.
    // e.g. ["*.wav", "file.flac", "-foo*"]
    static std::optional<FilepathSet> CreateFromPatterns(const std::vector<std::string> &parts,
                                                         bool recursive_directory_search,
                                                         std::string *error = nullptr);

    auto Size() const { return m_filepaths.size(); }

    auto begin() { return m_filepaths.begin(); }
    auto end() { return m_filepaths.end(); }
    auto begin() const { return m_filepaths.begin(); }
    auto end() const { return m_filepaths.end(); }

  private:
    FilepathSet() {}
    void AddNonExcludedPaths(const tcb::span<const fs::path> paths,
                             const std::vector<std::string> &exclude_patterns);
    void Add(const fs::path &path) { m_filepaths.insert(fs::canonical(path)); }
    void Add(const std::vector<fs::path> &paths) {
        for (const auto &p : paths) {
            Add(p);
        }
    }

    std::set<fs::path> m_filepaths {};
};
