#include "signet_interface.h"

#include <functional>

#include "doctest.hpp"

#include "audio_file_io.h"
#include "cli_formatter.h"
#include "commands/auto_tune/auto_tune.h"
#include "commands/convert/convert.h"
#include "commands/detect_pitch/detect_pitch.h"
#include "commands/embed_sampler_info/embed_sampler_info.h"
#include "commands/fade/fade.h"
#include "commands/filter/filters.h"
#include "commands/fix_pitch_drift/fix_pitch_drift_command.h"
#include "commands/folderise/folderise.h"
#include "commands/gain/gain.h"
#include "commands/move/move.h"
#include "commands/normalise/normalise.h"
#include "commands/pan/pan.h"
#include "commands/print_info/print_info.h"
#include "commands/rename/rename.h"
#include "commands/reverse/reverse.h"
#include "commands/sample_blend/sample_blend.h"
#include "commands/seamless_loop/seamless_loop.h"
#include "commands/trim/trim.h"
#include "commands/trim_silence/trim_silence.h"
#include "commands/tune/tune.h"
#include "commands/zcross_offset/zcross_offset.h"
#include "test_helpers.h"
#include "tests_config.h"
#include "version.h"

SignetInterface::SignetInterface() {
    m_commands.push_back(std::make_unique<AutoTuneCommand>());
    m_commands.push_back(std::make_unique<ConvertCommand>());
    m_commands.push_back(std::make_unique<DetectPitchCommand>());
    m_commands.push_back(std::make_unique<EmbedSamplerInfo>());
    m_commands.push_back(std::make_unique<FadeCommand>());
    m_commands.push_back(std::make_unique<FixPitchDriftCommand>());
    m_commands.push_back(std::make_unique<FolderiseCommand>());
    m_commands.push_back(std::make_unique<GainCommand>());
    m_commands.push_back(std::make_unique<HighpassCommand>());
    m_commands.push_back(std::make_unique<LowpassCommand>());
    m_commands.push_back(std::make_unique<MoveCommand>());
    m_commands.push_back(std::make_unique<NormaliseCommand>());
    m_commands.push_back(std::make_unique<PanCommand>());
    m_commands.push_back(std::make_unique<PrintInfoCommand>());
    m_commands.push_back(std::make_unique<RenameCommand>());
    m_commands.push_back(std::make_unique<ReverseCommand>());
    m_commands.push_back(std::make_unique<SampleBlendCommand>());
    m_commands.push_back(std::make_unique<SeamlessLoopCommand>());
    m_commands.push_back(std::make_unique<TrimCommand>());
    m_commands.push_back(std::make_unique<TrimSilenceCommand>());
    m_commands.push_back(std::make_unique<TuneCommand>());
    m_commands.push_back(std::make_unique<ZeroCrossOffsetCommand>());
}

int SignetInterface::Main(const int argc, const char *const argv[]) {
    CLI::App app {
        R"^^(Signet is a command-line program designed for bulk editing audio files. It has commands for converting, editing, renaming and moving WAV and FLAC files. It also features commands that generate audio files. Signet was primarily designed for people who make sample libraries, but its features can be useful for any type of bulk audio processing.)^^"};

    app.require_subcommand();
    app.set_help_all_flag("--help-all", "Print help message for all commands");
    app.formatter(std::make_shared<SignetCLIHelpFormatter>(SignetCLIHelpFormatter::OutputMode::CommandLine));
    app.name("signet");
    app.group("Commands");

    bool success_thrown = false;

    app.add_subcommand(
           "undo",
           "Undo any changes made by the last run of Signet; files that were overwritten are restored, new files that were created are destroyed, and files that were renamed are un-renamed. You can only undo once - you cannot keep going back in history.")
        ->final_callback([&]() {
            MessageWithNewLine("Signet", {}, "Undoing changes made by the last run of Signet...");
            m_backup.LoadBackup();
            MessageWithNewLine("Signet", {}, "Done.");
            success_thrown = true;
            throw CLI::Success();
        });

    app.add_flag_callback(
        "--version",
        [&]() {
            fmt::print("Signet version {}", SIGNET_VERSION);
#ifdef SIGNET_DEBUG
            fmt::print(" (debug)");
#endif
            fmt::print("\n");
            success_thrown = true;
            throw CLI::Success();
        },
        "Display the version of Signet");

    {
        auto make_docs = app.add_subcommand(
            "make-docs",
            "Creates a Github flavour markdown file containing the full CLI - based on running signet --help.");
        make_docs
            ->add_option("output-file", m_make_docs_filepath, "The filepath for the generated markdown file.")
            ->required();
        make_docs->final_callback([&]() {
            // Clear anything that was parsed, we want to fetch all of the subcommands, not just ones that
            // were parsed.
            app.clear();

            // Set a new formatter in Markdown mode
            SetFormatterRecursively(
                &app, std::make_shared<SignetCLIHelpFormatter>(SignetCLIHelpFormatter::OutputMode::Markdown));

            std::ofstream os(m_make_docs_filepath);

            os << "# Signet Usage\n";
            os << "\nThis is an auto-generated file based on the output of `signet --help`. It contains information about every feature of Signet.\n\n";

            auto all_commands_sorted = app.get_subcommands({});
            std::sort(all_commands_sorted.begin(), all_commands_sorted.end(),
                      [](const CLI::App *a, const CLI::App *b) { return a->get_name() < b->get_name(); });

            std::map<std::string, std::vector<std::string>> command_categories;
            command_categories["Signet Utility"] = {"undo", "clear-backup", "make-docs"};
            command_categories["Filepath"] = {"rename", "move", "folderise"};
            command_categories["Audio"] = {
                "auto-tune", "fade",    "fix-pitch-drift", "gain", "highpass",     "lowpass", "norm",
                "pan",       "reverse", "seamless-loop",   "trim", "trim-silence", "tune",    "zcross-offset",
            };
            command_categories["File Data"] = {"convert", "embed-sampler-info"};
            command_categories["Info"] = {"detect-pitch", "print-info"};
            command_categories["Generate"] = {"sample-blend"};

            // Sort each command category
            for (auto &c : command_categories) {
                std::sort(c.second.begin(), c.second.end());
            }

            for (auto cmd : all_commands_sorted) {
                bool found = false;
                for (auto c : command_categories) {
                    for (auto cmd_a : c.second) {
                        if (cmd_a == cmd->get_name()) {
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
                assert(found && "you must specify a category for this command");
            }

            // Print a contents section
            os << "- [General Usage](#General-Usage)\n";
            for (auto c : command_categories) {
                std::string link_name = c.first;
                Replace(link_name, " ", "-");
                os << fmt::format("- [{} Commands](#{}-Commands)\n", c.first, link_name);
                std::sort(c.second.begin(), c.second.end());
                for (auto &i : c.second) {
                    os << fmt::format("  - [{}](#sound-{})\n", i, i);
                    auto command = std::find_if(all_commands_sorted.begin(), all_commands_sorted.end(),
                                                [i](const CLI::App *cmd) { return cmd->get_name() == i; });
                    for (auto ss : (*command)->get_subcommands({})) {
                        os << fmt::format("    - [{}](#{})\n", ss->get_name(), ss->get_name());
                    }
                }
            }
            os << '\n';

            auto MakeAngleBracketWordsMarkdownCode = [](std::string markdown) {
                // There are commands have use a concept of a 'variable' which is a string instide angle
                // brackets. In order for them to show up nicely in the markdown, we wrap them in ` characters
                // to make them markdown 'code'.
                std::string result {};
                bool inside_code_block = false;
                for (auto l : Split(markdown, "\n", true)) {
                    if (Contains(l, "```")) inside_code_block = !inside_code_block;
                    if (!inside_code_block) {
                        result +=
                            std::regex_replace(std::string(l), std::regex("<[a-z0-9-]{2,}>"), "`$&`") + "\n";
                    } else {
                        result += std::string(l) + "\n";
                    }
                }
                return result;
            };

            os << "# General Usage\n";
            auto main_usage = MakeAngleBracketWordsMarkdownCode(app.help("", CLI::AppFormatMode::Normal));
            // Rather than include the text descriptions of the commands, we ignore anything from the heading
            // "commands" onwards and instead write our own commands section below.
            for (auto l : Split(main_usage, "\n", true)) {
                if (l == "## Commands:") break;
                std::string line {l};
                os << line << "\n";
            }

            for (auto c : command_categories) {
                os << fmt::format("# {} Commands\n", c.first);

                global_formatter_indent = 2;

                for (auto &i : c.second) {
                    auto command = std::find_if(all_commands_sorted.begin(), all_commands_sorted.end(),
                                                [i](const CLI::App *cmd) { return cmd->get_name() == i; });
                    os << "## :sound: " << (*command)->get_name() << "\n";
                    os << MakeAngleBracketWordsMarkdownCode((*command)->help("", CLI::AppFormatMode::All));
                }

                global_formatter_indent = 0;
            }

            success_thrown = true;
            MessageWithNewLine("Signet", {}, "Successfully written docs file");
            throw CLI::Success();
        });
    }

    app.add_subcommand(
           "clear-backup",
           "Deletes all temporary files created by Signet. These files are needed for the undo system and are saved to your OS's temporary folder. These files are cleared and new ones created every time you run Signet. This option is only really useful if you have just processed lots of files and you won't be using Signet for a long time afterwards. You cannot use undo directly after clearing the backup.")
        ->final_callback([&]() {
            MessageWithNewLine("Signet", {}, "Clearing all backed-up files...");
            m_backup.ClearBackup();
            MessageWithNewLine("Signet", {}, "Done.");
            success_thrown = true;
            throw CLI::Success();
        });

    app.add_flag_callback("--silent", []() { g_messages_enabled = false; }, "Disable all messages");

    app.add_flag_callback(
        "--warnings-are-errors", []() { g_warnings_as_errors = true; },
        "Attempt to exit Signet and return a non-zero value as soon as possible if a warning occurs.");

    app.add_flag("--recursive", m_recursive_directory_search,
                 "When the input is a directory, scan for files in it recursively.");

    auto input_files_option = app.add_option_function<std::vector<std::string>>(
        "input-files",
        [&](const std::vector<std::string> &input) {
            m_input_audio_files = AudioFiles(input, m_recursive_directory_search);
        },
        R"aa(The audio files to process. You can specify more than one of these. Each input-file you specify has to be a file, directory or a glob pattern. You can exclude a pattern by beginning it with a dash. e.g. "-*.wav" would exclude all .wav files that are in the current directory. If you specify a directory, all files within it will be considered input-files, but subdirectories will not be searched. You can use the --recursive flag to make signet search all subdirectories too.)aa");

    auto output_folder_option =
        app.add_option(
               "--output-folder", m_output_path,
               "Instead of overwriting the input files, put the processed audio files are put into the given output folder. Subfolders are not created within the output folder; all files are put at the same level. This option takes 1 argument - the path of the folder where the files should be moved to. You can specify this folder to be the same as any of the input folders, however, you will need to use the rename command to avoid overwriting the files. If the output folder does not already exist it will be created. Some commands do not allow this option - such as move.")
            ->check([&](const std::string &str) -> std::string {
                if (fs::exists(str) && !fs::is_directory(str)) {
                    return "The given output is a file that already exists.";
                }
                return {};
            });

    auto output_file_option =
        app.add_option(
               "--output-file", m_single_output_file,
               "Write to a single output file rather than overwrite the original. Only valid if there's only 1 input file. If the output file already exists it is overwritten. Directories are created. Some commands do not allow this option - such as move.")
            ->excludes(output_folder_option)
            ->check([&](const std::string &) -> std::string {
                if (m_input_audio_files.Size() != 1) {
                    return "You can only specify one input file when using --output-file";
                }
                return {};
            });

    for (auto &command : m_commands) {
        auto s = command->CreateCommandCLI(app);
        s->needs(input_files_option);
        s->final_callback([&] {
            struct FileEditState {
                int num_audio_edits, num_path_edits;
            };
            std::vector<FileEditState> initial_file_edit_state;
            initial_file_edit_state.reserve(m_input_audio_files.Size());
            for (const auto &f : m_input_audio_files) {
                initial_file_edit_state.push_back({f.NumTimesAudioChanged(), f.NumTimesPathChanged()});
            }

            MessageWithNewLine(command->GetName(), {}, "Starting processing");
            command->ProcessFiles(m_input_audio_files);
            command->GenerateFiles(m_input_audio_files, m_backup);

            int num_audio_edits = 0;
            int num_path_edits = 0;
            assert(initial_file_edit_state.size() == m_input_audio_files.Size());
            for (usize i = 0; i < initial_file_edit_state.size(); ++i) {
                const auto &f = m_input_audio_files[i];
                if (initial_file_edit_state[i].num_audio_edits != f.NumTimesAudioChanged()) ++num_audio_edits;
                if (initial_file_edit_state[i].num_path_edits != f.NumTimesPathChanged()) ++num_path_edits;
            }
            MessageWithNewLine(command->GetName(), {}, "Total audio files edited: {}", num_audio_edits);
            MessageWithNewLine(command->GetName(), {}, "Total audio file paths edited: {}", num_path_edits);
        });
        if (!command->AllowsOutputFolder()) {
            s->excludes(output_folder_option);
        }
        if (!command->AllowsSingleOutputFile()) {
            s->excludes(output_file_option);
        }
    }

    const auto PrintSuccess = []() {
        fmt::print(fmt::fg(fmt::terminal_color::green), "Signet completed successfully.\n");
    };

    try {
        app.parse(argc, argv);

        if (m_input_audio_files.GetNumFilesProcessed()) {
            if (m_output_path) {
                for (auto &f : m_input_audio_files) {
                    auto new_path = *m_output_path / f.GetPath().filename();
                    f.SetPath(new_path);
                }
            } else if (m_single_output_file) {
                REQUIRE(m_input_audio_files.Size() == 1);
                m_input_audio_files.begin()[0].SetPath(*m_single_output_file);
            }

            if (!m_input_audio_files.WriteFilesThatHaveBeenEdited(
                    m_backup, m_output_path || m_single_output_file ? true : false)) {
                return SignetResult::FailedToWriteFiles;
            }
        }

        if (m_input_audio_files.Size() == 0) {
            return SignetResult::NoFilesMatchingInput;
        } else if (m_input_audio_files.GetNumFilesProcessed() == 0) {
            return SignetResult::NoFilesWereProcessed;
        }

        PrintSuccess();
        return SignetResult::Success;
    } catch (const CLI::ParseError &e) {
        if (!success_thrown) PrintSignetHeading();
        if (e.get_exit_code() != 0) {
            std::stringstream out;
            std::stringstream error;
            auto result = app.exit(e, out, error);

            fmt::print(fg(fmt::color::red), "ERROR:\n{}{}", out.str(), error.str());
            return result;
        } else {
            std::stringstream help_text_stream;
            const auto result = app.exit(e, help_text_stream);
            fmt::print("{}", help_text_stream.str());

            PrintSuccess();
            return SignetResult::Success;
        }
    } catch (const SignetError &e) {
        fmt::print(fg(fmt::color::red) | fmt::emphasis::bold,
                   "{}. Processing has stopped. No files have been changed or saved.\n", e.what());
        return SignetResult::FatalErrorOcurred;
    } catch (const SignetWarning &e) {
        fmt::print(fg(fmt::color::red) | fmt::emphasis::bold,
                   "{}. Processing has stopped. No files have been changed or saved.\n", e.what());
        return SignetResult::WarningsAreErrors;
    }
}

TEST_CASE("[SignetInterface]") {
    SignetInterface signet;

    const auto in_file = std::string(TEST_DATA_DIRECTORY "/white-noise.wav");
    std::string test_folder = "test-folder";
    if (!fs::is_directory(test_folder)) {
        fs::create_directory(test_folder);
    }
    REQUIRE(fs::copy_file(TEST_DATA_DIRECTORY "/white-noise.wav", "test-folder/tf1.wav",
                          fs::copy_options::overwrite_existing));
    REQUIRE(fs::copy_file(TEST_DATA_DIRECTORY "/white-noise.wav", "test-folder/tf2.wav",
                          fs::copy_options::overwrite_existing));
    REQUIRE(fs::copy_file(TEST_DATA_DIRECTORY "/test.wav", "test-folder/test.wav",
                          fs::copy_options::overwrite_existing));

    SUBCASE("args") {
        SUBCASE("single file absolute filename") {
            const auto args = TestHelpers::StringToArgs {"signet test-folder/tf1.wav fade in 50smp"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("single file relative filename") {
            const auto args = TestHelpers::StringToArgs {"signet test-folder/../test-folder/tf1.wav norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("match all WAVs in a dir by using a wildcard") {
            const auto args = TestHelpers::StringToArgs {"signet test-folder/*wav norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
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

        SUBCASE("load undo") {
            const auto args = TestHelpers::StringToArgs {"signet undo"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }

        SUBCASE("multiple comma separated files") {
            const auto args =
                TestHelpers::StringToArgs {"signet test-folder/test.wav test-folder/tf1.wav norm -3"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);
        }
    }

    SUBCASE("undo") {
        SUBCASE("undo of tf1.wav") {
            auto args = TestHelpers::StringToArgs {"signet test-folder/tf1.wav trim start 50%"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

            auto f = ReadAudioFile("test-folder/tf1.wav");
            REQUIRE(f);
            auto starting_size = f->interleaved_samples.size();

            args = TestHelpers::StringToArgs {"signet undo"};
            REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

            f = ReadAudioFile("test-folder/tf1.wav");
            REQUIRE(f);
            REQUIRE(f->interleaved_samples.size() > starting_size);
        }

        SUBCASE("undo of tf1.wav and tf2.wav") {
            usize trimmed_size_tf1 = 0;
            usize trimmed_size_tf2 = 0;
            {
                const auto args = TestHelpers::StringToArgs {"signet test-folder/tf*.wav trim start 50%"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                {
                    auto f = ReadAudioFile("test-folder/tf1.wav");
                    REQUIRE(f);
                    trimmed_size_tf1 = f->interleaved_samples.size();

                    f = ReadAudioFile("test-folder/tf2.wav");
                    REQUIRE(f);
                    trimmed_size_tf2 = f->interleaved_samples.size();
                }
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet undo"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                {
                    auto f = ReadAudioFile("test-folder/tf1.wav");
                    REQUIRE(f);
                    REQUIRE(f->interleaved_samples.size() > trimmed_size_tf1);

                    f = ReadAudioFile("test-folder/tf2.wav");
                    REQUIRE(f);
                    REQUIRE(f->interleaved_samples.size() > trimmed_size_tf2);
                }
            }
        }

        SUBCASE("undo of renaming tf1.wav") {
            {
                const auto args = TestHelpers::StringToArgs {"signet test-folder/tf1.wav rename prefix foo_"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/foo_tf1.wav"));
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet undo"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                REQUIRE(!fs::exists("test-folder/foo_tf1.wav"));
            }
        }

        SUBCASE("undoing changing file format") {
            {
                const auto args =
                    TestHelpers::StringToArgs {"signet test-folder/tf1.wav convert file-format flac"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.flac"));
                REQUIRE(ReadAudioFile("test-folder/tf1.flac"));
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet undo"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                REQUIRE(ReadAudioFile("test-folder/tf1.wav"));
                REQUIRE(!fs::exists("test-folder/tf1.flac"));
            }
        }

        SUBCASE("undoing changing format and renaming") {
            {
                const auto args = TestHelpers::StringToArgs {
                    "signet test-folder/tf1.wav convert file-format flac rename prefix foo_"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/foo_tf1.flac"));
                REQUIRE(ReadAudioFile("test-folder/foo_tf1.flac"));
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet undo"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                REQUIRE(ReadAudioFile("test-folder/tf1.wav"));
                REQUIRE(!fs::exists("test-folder/foo_tf1.flac"));
            }
        }

        SUBCASE("undoing renaming and changing the format") {
            {
                const auto args = TestHelpers::StringToArgs {
                    "signet test-folder/tf1.wav rename prefix foo_ convert file-format flac"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/foo_tf1.flac"));
                REQUIRE(ReadAudioFile("test-folder/foo_tf1.flac"));
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet undo"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                REQUIRE(ReadAudioFile("test-folder/tf1.wav"));
                REQUIRE(!fs::exists("test-folder/foo_tf1.flac"));
            }
        }

        SUBCASE("undoing folderising") {
            {
                const auto args =
                    TestHelpers::StringToArgs {"signet test-folder/tf1.wav folderise .* folderise-output"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("folderise-output/tf1.wav"));
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet undo"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                REQUIRE(!fs::exists("folderise-output/tf1.wav"));
            }
        }

        SUBCASE("undoing renaming, changing the format and changing the data") {
            usize trimmed_size = 0;
            {
                const auto args = TestHelpers::StringToArgs {"signet test-folder/tf1.wav rename prefix "
                                                             "foo_ convert file-format flac trim start 50%"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/foo_tf1.flac"));
                const auto f = ReadAudioFile("test-folder/foo_tf1.flac");
                REQUIRE(f);
                trimmed_size = f->interleaved_samples.size();
                REQUIRE(!fs::exists("test-folder/tf1.wav"));
            }

            {
                const auto args = TestHelpers::StringToArgs {"signet undo"};
                REQUIRE(signet.Main(args.Size(), args.Args()) == 0);

                REQUIRE(fs::is_regular_file("test-folder/tf1.wav"));
                const auto f = ReadAudioFile("test-folder/tf1.wav");
                REQUIRE(f);
                REQUIRE(f->interleaved_samples.size() > trimmed_size);
                REQUIRE(!fs::exists("test-folder/foo_tf1.flac"));
            }
        }
    }
}
