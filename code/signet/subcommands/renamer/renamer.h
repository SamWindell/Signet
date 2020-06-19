#pragma once

#include <optional>

#include "subcommand.h"
#include "types.h"

class Renamer final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(const tcb::span<InputAudioFile> files) override;

  private:
    void AddToFolderMap(const fs::path &path);
    void ConstructAllAutomappings();

    class AutomapFolder {
      public:
        struct AutomapFile {
            std::string path;
            int root;
            int low;
            int high;
        };

        void AddFile(const fs::path &path, int root_note) {
            m_files.push_back({path.generic_string(), root_note});
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
            const auto p = path.generic_string();
            for (const auto &f : m_files) {
                if (p == f.path) {
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

    std::unordered_map<std::string, AutomapFolder> m_folder_map;
    std::optional<std::string> m_automap_pattern;
    std::optional<std::string> m_automap_out;

    std::optional<std::string> m_prefix;
    std::optional<std::string> m_suffix;
    std::optional<std::string> m_regex_pattern;
    std::string m_regex_replacement;

    int m_counter {};
};
