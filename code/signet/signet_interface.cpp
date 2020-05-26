#include "signet_interface.h"

#include <cassert>
#include <functional>

#include "audio_file.h"
#include "subcommands/fader/fader.h"
#include "subcommands/normalise/normaliser.h"
#include "subcommands/zcross_offsetter/zcross_offsetter.h"

SignetInterface::SignetInterface() {
    m_processors.push_back(new Fader());
    m_processors.push_back(new Normaliser());
    m_processors.push_back(new ZeroCrossingOffseter());
}

int SignetInterface::Main(const int argc, const char *argv[]) {
    CLI::App app {"Tools for processing audio files"};

    app.set_help_all_flag("--help-all", "Print help message for all subcommands");

    app.add_option("input-file-or-directory", m_input_filepath, "The file or directory to read from")
        ->required()
        ->check(CLI::ExistingPath);
    app.add_option("output-wave-filename", m_output_filepath,
                   "The filename to write to - only relevant if the input file is not a directory");
    app.add_flag("-r,--recursive-directory-search", m_recursive_directory_search,
                 "Search for files recursively in the given directory");
    app.add_flag("-d,--delete-input-files", m_delete_input_files,
                 "Delete the input files if the new file was successfully written");

    for (auto &p : m_processors) {
        p->AddCLI(app);
    }

    CLI11_PARSE(app, argc, argv);

    if (ghc::filesystem::is_directory(m_input_filepath)) {
        if (!m_output_filepath.empty()) {
            FatalErrorWithNewLine("the input path is a directory, there must be no output filepath - output "
                                  "files will be placed adjacent to originals");
        }
    } else {
        if (m_recursive_directory_search) {
            WarningWithNewLine("input path is a file, ignoring the recursive flag");
        }
        if (ghc::filesystem::is_directory(m_output_filepath)) {
            FatalErrorWithNewLine("output filepath cannot be a directory if the input filepath is a file");
        }
    }

    for (auto &p : m_processors) {
        p->Run(*this);
    }
    return 0;
}

template <typename DirectoryIterator>
static void ForEachAudioFileInDirectory(const std::string &directory,
                                        std::function<void(const ghc::filesystem::path &)> callback) {
    std::vector<ghc::filesystem::path> paths;
    for (const auto &entry : DirectoryIterator(directory)) {
        const auto &path = entry.path();
        const auto ext = path.extension();
        if (ext == ".flac" || ext == ".wav") {
            paths.push_back(path);
        }
    }

    // We do this in a separate loop because the callback might write a file. We do not want to then process
    // it again.
    for (const auto &path : paths) {
        callback(path);
    }
    std::cout << "Processed " << paths.size() << (paths.size() == 1 ? " file\n\n" : " files\n\n");
}

static void ForEachAudioFileInDirectory(const std::string &directory,
                                        const bool recursive,
                                        std::function<void(const ghc::filesystem::path &)> callback) {
    if (recursive) {
        ForEachAudioFileInDirectory<ghc::filesystem::recursive_directory_iterator>(directory, callback);
    } else {
        ForEachAudioFileInDirectory<ghc::filesystem::directory_iterator>(directory, callback);
    }
}

void SignetInterface::ProcessAllFiles(Processor &processor) {
    if (IsProcessingMultipleFiles()) {
        ForEachAudioFileInDirectory(
            m_input_filepath, m_recursive_directory_search,
            [&](const ghc::filesystem::path &path) { ProcessFile(processor, path, {}); });
    } else {
        ProcessFile(processor, m_input_filepath, m_output_filepath);
    }
}

void SignetInterface::ProcessFile(Processor &processor,
                                  const ghc::filesystem::path input_filepath,
                                  ghc::filesystem::path output_filepath) {
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
        if (const auto new_audio_file = processor.Process(*audio_file, output_filepath)) {
            if (!WriteWaveFile(output_filepath, *new_audio_file)) {
                FatalErrorWithNewLine("could not write the wave file ", output_filepath);
            }
            std::cout << "Successfully wrote file " << output_filepath << "\n";

            if (m_delete_input_files && input_filepath != output_filepath) {
                std::cout << "Deleting file " << input_filepath << "\n";
                ghc::filesystem::remove(input_filepath);
            }
        }
    }

    std::cout << "\n";
}
