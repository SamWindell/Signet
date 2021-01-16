#pragma once

#include <optional>
#include <regex>
#include <vector>

#include "CLI11.hpp"
#include "filesystem.hpp"
#include "span.hpp"

#include "audio_files.h"

class AutomapFolder {
  public:
    struct AutomapFile {
        fs::path path;
        int root;
        int low;
        int high;
        std::vector<std::string> regex_groups;
    };

    void AddFile(const fs::path &path, int root_note, const std::smatch &match) {
        AutomapFile file {};
        file.path = path;
        file.root = root_note;
        for (usize i = 0; i < match.size(); ++i) {
            file.regex_groups.push_back(match[i].str());
        }
        m_files.push_back(file);
    }

    void Automap() {
        std::sort(m_files.begin(), m_files.end(),
                  [](const auto &a, const auto &b) { return a.root < b.root; });

        if (m_files.size() == 1) {
            m_files[0].low = 0;
            m_files[0].high = 127;
        } else {
            for (usize i = 0; i < m_files.size(); ++i) {
                if (i == 0) {
                    MapFile(m_files[i], {{}, -1, -1, -1}, m_files[i + 1]);
                } else if (i == m_files.size() - 1) {
                    MapFile(m_files[i], m_files[i - 1], {{}, 128, 128, 128});
                } else {
                    MapFile(m_files[i], m_files[i - 1], m_files[i + 1]);
                }
            }
            m_files.back().high = 127;
        }
    }

    const AutomapFile *GetFile(const fs::path &path) {
        for (const auto &f : m_files) {
            if (path == f.path) {
                return &f;
            }
        }
        return nullptr;
    }

  private:
    void MapFile(AutomapFile &file, const AutomapFile &prev, const AutomapFile &next) {
        file.low = prev.high + 1;
        file.high = file.root + (next.root - file.root) / 2;
    }

    std::vector<AutomapFile> m_files;
};

class AutoMapper {
  public:
    void CreateCLI(CLI::App &renamer);
    void InitialiseProcessing(AudioFiles &files);
    bool Rename(const EditTrackedAudioFile &file, std::string &filename);

  private:
    void AddToFolderMap(const fs::path &path);
    void ConstructAllAutomappings();

    std::map<fs::path, AutomapFolder> m_folder_map;
    std::optional<std::string> m_automap_pattern;
    std::optional<std::string> m_automap_out;
    int m_root_note_regex_group {};
};
