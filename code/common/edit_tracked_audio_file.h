#pragma once

#include "audio_file_io.h"
#include "common.h"
#include "string_utils.h"

// Changes made to the data, path or format are tracked, and the data is only loaded when it is requested
struct EditTrackedAudioFile {
    EditTrackedAudioFile(const fs::path &path) : m_path(path), m_original_path(path) {}

    AudioData &GetWritableAudio() {
        ++m_file_edited;
        return const_cast<AudioData &>(GetAudio());
    }

    const AudioData &GetAudio() {
        if (!m_file_loaded && m_file_valid) {
            if (const auto data = ReadAudioFile(m_original_path)) {
                SetAudioData(*data);
            } else {
                ErrorWithNewLine("Signet", m_original_path, "could not load audio");
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

    bool AudioChanged() const { return m_file_edited && m_file_valid; }
    bool PathChanged() const { return m_path_edited; }
    bool FormatChanged() const { return m_file_loaded && m_original_file_format != m_data.format; }

    void SetAudioData(const AudioData &data) {
        m_data = data;
        m_original_file_format = m_data.format;
        m_file_loaded = true;
    }

    int NumTimesAudioChanged() const { return m_file_edited; }
    int NumTimesPathChanged() const { return m_path_edited; }

    const fs::path &OriginalPath() const { return m_original_path; }
    std::string OriginalFilename() const { return GetJustFilenameWithNoExtension(OriginalPath()); }

  private:
    AudioFileFormat m_original_file_format {};
    fs::path m_path {};
    AudioData m_data {};
    bool m_file_loaded = false;
    bool m_file_valid = true;

    int m_file_edited = 0;
    int m_path_edited = 0;

    fs::path m_original_path;
};
