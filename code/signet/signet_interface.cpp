#include "signet_interface.h"

#include <functional>

#include "doctest.hpp"

#include "audio_file.h"
#include "subcommands/converter/converter.h"
#include "subcommands/fader/fader.h"
#include "subcommands/normalise/normaliser.h"
#include "subcommands/zcross_offsetter/zcross_offsetter.h"
#include "test_helpers.h"
#include "tests_config.h"

SignetInterface::SignetInterface() {
    m_subcommands.push_back(std::make_unique<Fader>());
    m_subcommands.push_back(std::make_unique<Normaliser>());
    m_subcommands.push_back(std::make_unique<ZeroCrossingOffsetter>());
    m_subcommands.push_back(std::make_unique<Converter>());
}

int SignetInterface::Main(const int argc, const char *const argv[]) {
    CLI::App app {"Tools for processing audio files"};

    app.require_subcommand();
    app.set_help_all_flag("--help-all", "Print help message for all subcommands");
    app.add_flag_callback(
        "--load-backup",
        [this]() {
            MessageWithNewLine("Signet", "Loading most recent backup...");
            m_backup.LoadBackup();
            MessageWithNewLine("Signet", "Done.");
            throw CLI::Success();
        },
        "Load the most recent backup");

    app.add_option("input-file-or-directory", m_input_filepath_pattern, "The file or directory to read from")
        ->required();
    app.add_option("output-wave-filename", m_output_filepath,
                   "The filename to write to - only relevant if the input file is not a directory");

    std::vector<CLI::App *> subcommand_clis;
    for (auto &subcommand : m_subcommands) {
        subcommand_clis.push_back(subcommand->CreateSubcommandCLI(app));
    }

    CLI11_PARSE(app, argc, argv);

    m_backup.ResetBackup(); // if we have gotten here we must not be wanting to load a backup

    if (IsProcessingMultipleFiles()) {
        if (m_output_filepath) {
            ErrorWithNewLine("the input path is a directory or pattern, there must be no output filepath");
            return 1;
        }
    } else {
        if (m_output_filepath) {
            if (ghc::filesystem::is_directory(*m_output_filepath)) {
                ErrorWithNewLine("output filepath cannot be a directory if the input filepath is a file");
                return 1;
            } else if (m_output_filepath->extension() != ".wav" &&
                       m_output_filepath->extension() != ".flac") {
                ErrorWithNewLine("only WAV files can be written");
                return 1;
            }
        }
    }

    m_input_filepath_pattern.BuildAllMatchesFilenames();
    if (!m_input_filepath_pattern.GetAllMatchedFilenames().Size()) {
        ErrorWithNewLine("There are no matching files for the pattern ",
                         m_input_filepath_pattern.GetWholePattern());
        return 1;
    }

    for (const auto &path : m_input_filepath_pattern.GetAllMatchedFilenames()) {
        if (auto file = ReadAudioFile(path)) {
            m_all_files.push_back({*file, path});
        }
    }

    for (auto &subcommand_cli : app.get_subcommands()) {
        Subcommand *subcommand = nullptr;
        for (usize i = 0; i < subcommand_clis.size(); ++i) {
            if (subcommand_clis[i] == subcommand_cli) {
                subcommand = m_subcommands[i].get();
            }
        }

        if (subcommand) {
            subcommand->Run(*this);
        }
    }

    if (m_num_files_processed) {
        for (auto &[file, path] : m_all_files) {
            auto filepath = path;

            if (m_output_filepath) {
                filepath = *m_output_filepath;
            } else {
                m_backup.AddFileToBackup(filepath);
            }

            if (!WriteAudioFile(filepath, file)) {
                ErrorWithNewLine("could not write the wave file ", filepath);
            }
        }
    }

    return m_num_files_processed != 0 ? 0 : 1;
}

void SignetInterface::ProcessAllFiles(Subcommand &subcommand) {
    for (auto &[file, path] : m_all_files) {
        if (subcommand.Process(file)) {
            m_num_files_processed++;
        }
    }
}

TEST_CASE("[SignetInterface]") {
    SignetInterface signet;

    SUBCASE("args") {
        const auto in_file = std::string(TEST_DATA_DIRECTORY "/white-noise.wav");

        std::string test_folder = "test-folder";
        if (!ghc::filesystem::is_directory(test_folder)) {
            ghc::filesystem::create_directory(test_folder);
        }

        SUBCASE("single file absolute filename writing to output file") {
            const auto args =
                TestHelpers::StringToArgs {"signet " + in_file + " test-folder/test-out.wav fade in 50smp"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("single file relative filename overwrite") {
            const auto args =
                TestHelpers::StringToArgs {"signet test-folder/../test-folder/test-out.wav norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("single file with single output that is not a wav or flac") {
            const auto args =
                TestHelpers::StringToArgs {"signet " + in_file + " test-folder/test-out.ogg fade in 50smp"};
            REQUIRE(signet.Main(args.Size(), args.Args()) != 0);
        }

        SUBCASE("when the input file is a single file the output cannot be a directory") {
            const auto args =
                TestHelpers::StringToArgs {"signet test-folder/test-out.wav test-folder norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) != 0);
        }

        SUBCASE("match all WAVs in a dir by using a wildcard") {
            const auto args = TestHelpers::StringToArgs {"signet test-folder/*wav norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("when the input path is a pattern there cannot be an output file") {
            const auto args = TestHelpers::StringToArgs {"signet test-folder/*.wav output.wav norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) != 0);
        }

        SUBCASE("when input path is a patternless directory scan all files in that") {
            const auto wildcard = test_folder;
            const auto args = TestHelpers::StringToArgs {"signet test-folder norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("when input path is a patternless directory with ending slash scan all files in that") {
            const auto args = TestHelpers::StringToArgs {"signet test-folder/ norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("load backup") {
            const auto args = TestHelpers::StringToArgs {"signet --load-backup"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("multiple comma separated files") {
            const auto args =
                TestHelpers::StringToArgs {"signet test-folder/test.wav,test-folder/test-out.wav norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("read and write a flac file") {
            const auto args =
                TestHelpers::StringToArgs {"signet " + std::string(TEST_DATA_DIRECTORY "/test.flac") +
                                           " test-folder/test-out.flac fade in 50smp"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }
    }
}
