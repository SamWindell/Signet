#include "signet_interface.h"

#include <functional>

#include "doctest.hpp"

#include "audio_file.h"
#include "subcommands/auto_tuner/auto_tuner.h"
#include "subcommands/converter/converter.h"
#include "subcommands/fader/fader.h"
#include "subcommands/normalise/normaliser.h"
#include "subcommands/pitch_detector/pitch_detector.h"
#include "subcommands/renamer/renamer.h"
#include "subcommands/silence_remover/silence_remover.h"
#include "subcommands/trimmer/trimmer.h"
#include "subcommands/tuner/tuner.h"
#include "subcommands/zcross_offsetter/zcross_offsetter.h"
#include "test_helpers.h"
#include "tests_config.h"

SignetInterface::SignetInterface() {
    m_subcommands.push_back(std::make_unique<Fader>());
    m_subcommands.push_back(std::make_unique<Normaliser>());
    m_subcommands.push_back(std::make_unique<ZeroCrossingOffsetter>());
    m_subcommands.push_back(std::make_unique<Converter>());
    m_subcommands.push_back(std::make_unique<SilenceRemover>());
    m_subcommands.push_back(std::make_unique<Trimmer>());
    m_subcommands.push_back(std::make_unique<PitchDetector>());
    m_subcommands.push_back(std::make_unique<Tuner>());
    m_subcommands.push_back(std::make_unique<AutoTuner>());
    m_subcommands.push_back(std::make_unique<Renamer>());
}

int SignetInterface::Main(const int argc, const char *const argv[]) {
    CLI::App app {"Audio file processor"};

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

    app.add_flag("--recursive-folder-search", m_recursive_directory_search,
                 "When the input is a directory, scan for files in it recursively");

    app.add_option_function<std::string>(
           "input-files",
           [&](const std::string &input) {
               m_input_audio_files = InputAudioFiles(input, m_recursive_directory_search);
           },
           "The file, directory or glob pattern to process")
        ->required()
        ->type_name("STRING - a file, directory or glob pattern. To use multiple, separate each one with a "
                    "comma. You can exclude a pattern too by beginning it with a -. e.g. \"-*.wav\" to "
                    "exclude all .wav files.");

    app.add_option("output-filename", m_output_filepath,
                   "The filename to write to - only relevant if the input is a single file")
        ->check([&](const std::string &str) {
            MessageWithNewLine("Signet", "Checking if output file ", str, " is valid");
            if (!m_input_audio_files.IsSingleFile()) {
                return "the input path is a directory or pattern, there must be no output filepath";
            } else {
                fs::path path(str);
                if (fs::is_directory(path)) {
                    return "output filepath cannot be a directory if the input filepath is a file";
                } else if (path.extension() != ".wav" && path.extension() != ".flac") {
                    return "only WAV or FLAC files can be written";
                }
            }
            return "";
        });

    std::vector<CLI::App *> subcommand_clis;
    for (auto &subcommand : m_subcommands) {
        auto s = subcommand->CreateSubcommandCLI(app);
        s->final_callback([&] { subcommand->Run(*this); });
    }

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        if (e.get_exit_code() != 0) {
            std::atexit([]() { std::cout << rang::style::reset; });
            std::cout << rang::fg::red;
        }
        return app.exit(e);
    }

    m_backup.ResetBackup(); // if we have gotten here we must not be wanting to load a backup

    if (m_num_files_processed) {
        {
            std::unordered_set<std::string> files_set;
            bool file_conflicts = false;
            for (const auto &f : m_input_audio_files.GetAllFiles()) {
                const auto generic = f.path.generic_string();
                if (files_set.find(generic) != files_set.end()) {
                    ErrorWithNewLine("filepath ", generic, " would have the same filename as another file");
                    file_conflicts = true;
                }
                files_set.insert(generic);
            }
            if (file_conflicts) {
                ErrorWithNewLine(
                    "files could be unexpectedly overwritten, please review your renaming settings, "
                    "no action will be taken now");
                return 1;
            }
        }

        for (const auto &file : m_input_audio_files.GetAllFiles()) {
            if (file.file_edited) {
                auto filepath = file.path;
                if (m_output_filepath) {
                    filepath = *m_output_filepath;
                } else {
                    m_backup.AddFileToBackup(filepath);
                }

                if (!WriteAudioFile(filepath, file.file)) {
                    ErrorWithNewLine("could not write the wave file ", filepath);
                }
            }

            if (file.renamed) {
                if (!m_output_filepath) {
                    MessageWithNewLine("Signet", "Renaming the file ", file.original_path, " to ", file.path);
                } else {
                    MessageWithNewLine("Signet",
                                       "The output name has been specified but also has been renamed by the "
                                       "subcommands, we will rename the output name from ",
                                       file.original_path, " to ", file.path);
                }
                fs::rename(file.original_path, file.path);
            }
        }
    }

    return (m_num_files_processed != 0) ? 0 : 1;
}

void SignetInterface::ProcessAllFiles(Subcommand &subcommand) {
    for (auto &file : m_input_audio_files.GetAllFiles()) {
        if (subcommand.ProcessAudio(file.file, file.new_filename)) {
            file.file_edited = true;
        }
        if (subcommand.ProcessFilename(file.path, file.file)) {
            file.renamed = true;
        }
        if (file.renamed || file.file_edited) {
            m_num_files_processed++;
        }
    }
}

TEST_CASE("[SignetInterface]") {
    SignetInterface signet;

    SUBCASE("args") {
        const auto in_file = std::string(TEST_DATA_DIRECTORY "/white-noise.wav");

        std::string test_folder = "test-folder";
        if (!fs::is_directory(test_folder)) {
            fs::create_directory(test_folder);
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
