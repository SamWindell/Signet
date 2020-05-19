#include <assert.h>

#include "CLI11.hpp"
#include "common.h"

struct Noramaliser {
    void CreateCLI(CLI::App &app) {
        app.add_option("input-file-or-directory", m_input_filepath, "The file or directory to read from")
            ->required()
            ->check(CLI::ExistingPath);
        app.add_option("output-wave-filename", m_output_filepath,
                       "The filename to write to - only relevant if the input file is not a directory");
        app.add_flag("-r,--recursive-directory-search", m_recursive_directory_search,
                     "Search for files recursively in the given directory");
        app.add_flag("-d,--delete-input-files", m_delete_input_files,
                     "Delete the input files if the new file was successfully written");
        app.add_option("-t,--target_decibels", m_target_decibels,
                       "The target level in decibels to convert the sample(s) to");
        app.add_flag("-c,--common_gain", m_common_gain,
                     "When using on a directory, amplifiy all the samples by the same amount");
    }

    static float GetMaxValueInBuffer(const AudioFile &buffer) {
        float max_value = 0;
        for (const auto s : buffer.interleaved_samples) {
            const auto magnitude = std::abs(s);
            if (magnitude > max_value) {
                max_value = magnitude;
            }
        }
        return max_value;
    }

    AudioFile PerformNormalisation(const AudioFile &input_audio, float max_value) const {
        if (max_value == 0) {
            max_value = GetMaxValueInBuffer(input_audio);
        }

        const auto target_amp = DBToAmp(m_target_decibels);
        const auto gain = target_amp / max_value;
        std::cout << "Max value is " << max_value << ", applying a gain of " << gain << "\n";

        AudioFile result = input_audio;
        for (auto &s : result.interleaved_samples) {
            s *= gain;
        }

        return result;
    }

    float ReadFileForCommonGain(const ghc::filesystem::path &input_filepath,
                                float common_gain_max_value) const {
        if (const auto audio_file = ReadAudioFile(input_filepath)) {
            const auto max = GetMaxValueInBuffer(*audio_file);
            if (max > common_gain_max_value) {
                common_gain_max_value = max;
            }
        } else {
            WarningWithNewLine("Could not read the file ", input_filepath);
        }
        return common_gain_max_value;
    }

    void NoramliseFile(const ghc::filesystem::path &input_filepath,
                       ghc::filesystem::path output_filepath,
                       float max_value = 0) const {
        if (output_filepath.empty()) {
            output_filepath = input_filepath;
            if (!m_delete_input_files) {
                output_filepath.replace_extension();
                output_filepath += "(edited)";
            }
            output_filepath.replace_extension(".wav");
        }
        assert(!input_filepath.empty());

        if (const auto audio_file = ReadAudioFile(input_filepath)) {
            const auto new_audio_file = PerformNormalisation(*audio_file, max_value);
            // if (!WriteWaveFile(output_filepath, new_audio_file)) {
            //     FatalErrorWithNewLine("could not write the wave file ", output_filepath);
            // }
            std::cout << "Successfully wrote file " << output_filepath << "\n";

            if (m_delete_input_files && input_filepath != output_filepath) {
                std::cout << "Deleting file " << input_filepath << "\n";
                // ghc::filesystem::remove(input_filepath);
            }
        }

        std::cout << "\n";
    }

    void Process() {
        if (ghc::filesystem::is_directory(m_input_filepath)) {
            if (!m_output_filepath.empty()) {
                FatalErrorWithNewLine(
                    "the input path is a directory, there must be no output filepath - output "
                    "files will be placed adjacent to originals");
            }

            if (!m_common_gain) {
                ForEachAudioFileInDirectory(
                    m_input_filepath, m_recursive_directory_search,
                    [this](const ghc::filesystem::path &path) { NoramliseFile(path, {}); });
            } else {
                float common_gain_max_value = 0;

                ForEachAudioFileInDirectory(
                    m_input_filepath, m_recursive_directory_search, [&](const ghc::filesystem::path &path) {
                        common_gain_max_value = ReadFileForCommonGain(path, common_gain_max_value);
                        std::cout << "common_gain_max_value " << common_gain_max_value << "\n\n";
                    });

                ForEachAudioFileInDirectory(m_input_filepath, m_recursive_directory_search,
                                            [&](const ghc::filesystem::path &path) {
                                                NoramliseFile(path, {}, common_gain_max_value);
                                            });
            }
        } else {
            if (m_recursive_directory_search) {
                WarningWithNewLine("input path is a file, ignoring the recursive flag");
            }
            if (m_common_gain) {
                WarningWithNewLine("input path is a file, ignoring the common gain flag");
            }
            if (ghc::filesystem::is_directory(m_output_filepath)) {
                FatalErrorWithNewLine(
                    "output filepath cannot be a directory if the input filepath is a file");
            }
            NoramliseFile(m_input_filepath, m_output_filepath);
        }
    }

  private:
    bool m_delete_input_files = false;
    ghc::filesystem::path m_input_filepath;
    ghc::filesystem::path m_output_filepath;
    bool m_recursive_directory_search = false;
    bool m_common_gain = false;
    float m_target_decibels = 0.0f;
};

int main(const int argc, const char *argv[]) {
    CLI::App app {"Normalise a sample to a certain level"};
    Noramaliser normaliser;
    normaliser.CreateCLI(app);
    CLI11_PARSE(app, argc, argv);

    normaliser.Process();
    return 0;
}
