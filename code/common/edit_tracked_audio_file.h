#pragma once
#include "audio_file.h"
#include "common.h"
#include "string_utils.h"

#include <map>

struct EditTrackedAudioFile;
using FolderMapType = std::map<fs::path, std::vector<EditTrackedAudioFile *>>;

// Changes made to the data, path or format are tracked, and the data is only loaded when it is requested
struct EditTrackedAudioFile {
    EditTrackedAudioFile(const fs::path &path)
        : filename(GetJustFilenameWithNoExtension(path)), original_path(path), m_path(path) {}

    AudioData &GetWritableAudio() {
        ++m_file_edited;
        return const_cast<AudioData &>(GetAudio());
    }

    const AudioData &GetAudio() {
        if (!m_file_loaded && m_file_valid) {
            if (const auto data = ReadAudioFile(original_path)) {
                LoadAudioData(*data);
            } else {
                ErrorWithNewLine("could not load audio file ", original_path);
                m_file_valid = false;
            }
        }
        return m_data;
    }

    const fs::path &GetPath() const { return m_path; }

    void SetPath(const fs::path &path) {
        ++m_path_edited;
        m_path = path;
    }

    const std::string filename;
    const fs::path original_path;

    bool AudioChanged() const { return m_file_edited && m_file_valid; }
    bool FilepathChanged() const { return m_path_edited; }
    bool FormatChanged() const { return m_file_loaded && m_original_file_format != m_data.format; }

    void LoadAudioData(const AudioData &data) {
        m_data = data;
        m_original_file_format = m_data.format;
        m_file_loaded = true;
    }

    int NumTimesAudioChanged() const { return m_file_edited; }
    int NumTimesPathChanged() const { return m_path_edited; }

  private:
    AudioFileFormat m_original_file_format {};
    fs::path m_path {};
    AudioData m_data {};
    bool m_file_loaded = false;
    bool m_file_valid = true;

    int m_file_edited = 0;
    int m_path_edited = 0;
};
