#include "auto_tune.h"

#include <regex>
#include <unordered_map>

#include "CLI11.hpp"
#include "doctest.hpp"

#include "audio_files.h"
#include "common.h"
#include "midi_pitches.h"

CLI::App *AutoTuneCommand::CreateCommandCLI(CLI::App &app) {
    auto auto_tune = app.add_subcommand(
        "auto-tune",
        "Tunes the file(s) to their nearest detected musical pitch. For example, a file with a detected pitch of 450Hz will be tuned to 440Hz (A4). The whole audio is analysed, and the most frequent and prominent pitch is determined. The whole audio is then retuned as if by using Signet's tune command (i.e. sped up or slowed down). This command works surprising well for almost any type of sample - transparently shifting it by the smallest amount possible to be more musically in-tune.");
    auto_tune
        ->add_option(
            "--sample-sets", m_sample_set_args,
            R"foo(Rather than auto-tune each file individually, identify sets of files and tune them all the items in each set in an identical manner based on a single authority file in that set. 
    
For example, you might have a set of samples recorded with different microphones; you can use this tool to tune all samples based on the close mic - eliminating issues when auto-tune processes each file slightly differently.
    
To allow for batch processing (as is the goal of Signet), this option is flexible and therefore requires a little explanation.

This option requires 2 arguments. 

The first argument is a regex pattern that will be used to identify sample sets from all of the file names (not including folders or extension). This must capture a single regex group. The bit that you capture is the bit of text that is different for each name in the set.

For example, take a folder of files like this:
sample-C2-close.flac
sample-C2-room.flac
sample-C2-ambient.flac
sample-D2-close.flac
sample-D2-room.flac
sample-D2-ambient.flac

Close, room and ambient are names of commonly used mic positions. They are recordings of the same thing; in a sampler we would want them to all be tuned identically.

The differentiator (first arg) should be ".*(close|room|ambient).*". 

This is because "close|room|ambient" is the ONLY bit that changes between the file names of each set. This option does not work if samples in a set have multiple parts that are different.

The second argument required for this command is used to determine what should be the authority for the auto-tune. This is a string that should match against whatever we have captured in the first argument. In this example case, it would be the word "close", because we want the close mics to be the authority.

Putting it all together, here's what the full command would look like for our example:

signet sample-* auto-tune --sample-sets ".*(close|room|ambient).*" "close"

The entire folder of different mic positions can be processed in a single command.)foo")
        ->expected(2);

    auto_tune->add_option_function<std::string>(
        "--sample-set",
        [this](const std::string &str) {
            m_sample_set_args.clear();
            m_sample_set_args.push_back("(.*)");
            m_sample_set_args.push_back(str);
        },
        R"foo("Rather than auto-tune each file individually, process all the files in an identical manner based on a single authority file. Takes 1 argument: the name (without folders or extension) of the file that should be the authority - all of the files will be tuned by the same amount based on this file.
    
This is the same as --sample-sets but just takes a single filename for all of the files (rather than allowing multiple sets to be identified using a regex pattern)foo");

    return auto_tune;
}

void AutoTuneCommand::ProcessFiles(AudioFiles &files) {
    if (m_sample_set_args.size() == 0) {
        for (auto &f : files) {
            if (const auto pitch = f.GetAudio().DetectPitch()) {
                const auto closest_musical_note = FindClosestMidiPitch(*pitch);
                const double cents = GetCentsDifference(*pitch, closest_musical_note.pitch);
                if (std::abs(cents) < 1) {
                    MessageWithNewLine(GetName(), "Sample is already in tune: {}",
                                       closest_musical_note.ToString());
                    continue;
                }
                MessageWithNewLine(GetName(), "Changing pitch from {} to {}", *pitch,
                                   closest_musical_note.ToString());
                f.GetWritableAudio().ChangePitch(cents);
            } else {
                MessageWithNewLine(GetName(), "No pitch could be found");
            }
        }
    } else {
        std::regex re {m_sample_set_args[0]};
        const auto &authority_matcher = m_sample_set_args[1];

        MessageWithNewLine(GetName(), "Performing auto-tune on sets of samples");
        std::unordered_map<std::string, std::vector<EditTrackedAudioFile *>> sets;

        std::smatch match;
        for (auto &f : files) {
            const auto filename = GetJustFilenameWithNoExtension(f.GetPath());

            std::string replaced = filename;
            if (std::regex_match(filename, match, re)) {
                if (match.size() == 2) {
                    replaced.assign(filename.begin(), match[1].first);
                    replaced.append(1, '*');
                    replaced.append(match[1].second, filename.end());
                }
            }

            auto &arr = sets[(f.GetPath().parent_path() / fs::path(replaced)).generic_string()];
            arr.push_back(&f);
        }

        for (auto &set : sets) {
            const auto human_set_name = GetJustFilenameWithNoExtension(set.first);

            EditTrackedAudioFile *authority_file = nullptr;
            for (auto &f : set.second) {
                const auto filename = GetJustFilenameWithNoExtension(f->GetPath());
                assert(std::regex_match(filename, match, re));
                assert(match.size() == 2);
                if (match[1] == authority_matcher) {
                    authority_file = f;
                    break;
                }
            }

            if (authority_file) {
                MessageWithNewLine(
                    GetName(),
                    "Processing sample-set \"{}\" (size {}) all with the same settings, using \"{}\" as the authority",
                    human_set_name, set.second.size(), authority_matcher);

                if (const auto pitch = authority_file->GetAudio().DetectPitch()) {
                    const auto closest_musical_note = FindClosestMidiPitch(*pitch);
                    const double cents = GetCentsDifference(*pitch, closest_musical_note.pitch);
                    if (std::abs(cents) < 1) {
                        MessageWithNewLine(GetName(), "Sample set is already in tune: {}",
                                           closest_musical_note.ToString());
                        continue;
                    }
                    MessageWithNewLine(GetName(), "Changing pitch from {} to {}", *pitch,
                                       closest_musical_note.ToString());

                    for (auto &f : set.second) {
                        f->GetWritableAudio().ChangePitch(cents);
                    }
                } else {
                    MessageWithNewLine(GetName(), "No pitch could be found for sample set {}", set.first);
                }
            } else {
                WarningWithNewLine(
                    GetName(),
                    "Failed to process sample-set because the authority file could not be identified\nFile: \"{}\"\nAuthority: \"{}\"",
                    human_set_name, authority_matcher);
            }
        }
    }
}
