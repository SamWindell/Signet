#include "identical_processing_set.h"

#include <regex>

#include "CLI11.hpp"

void IdenticalProcessingSet::AddCli(CLI::App &command) {
    command
        .add_option(
            "--sample-sets", m_sample_set_args,
            R"foo(Rather than process each file individually, identify sets of files and process the files in each set in an identical manner based on a single authority file in that set. 
    
For example, you might have a set of samples of something recorded simultaneously with different microphones; you can use this tool to process all of the samples in the same way based on the close mic.
    
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

In this example, close, room and ambient are names of commonly used mic positions. Each mic is capturing the same sound source at the same time; we therefore want any processing done on these files to be identical.

The differentiator (first arg) should be ".*(close|room|ambient).*". 

This is because "close|room|ambient" is the ONLY bit that changes between the file names of each set. This option does not work if samples in a set have multiple parts that are different.

The second argument required for this command is used to determine what should be the authority for the processing. This is a string that should match against whatever we have captured in the first argument. In this example case, it would be the word "close", because we want the close mics to be the authority.

Putting it all together, here's what the full command would look like for our example:

signet sample-* process --sample-sets ".*(close|room|ambient).*" "close"

The entire folder of different mic positions can be processed in a single command. For a simpler version of this option, see --authority-file.)foo")
        ->expected(2);

    command.add_option_function<std::string>(
        "--authority-file",
        [this](const std::string &str) {
            m_sample_set_args.clear();
            m_sample_set_args.push_back("(.*)");
            m_sample_set_args.push_back(str);
        },
        R"foo(Rather than process each file individually, process all of the files in an identical manner based on a single authority file. This takes 1 argument: the name (without folders or extension) of the file that should be the authority.
    
This is the same as --sample-sets, but just takes a single filename for all of the files (rather than allowing multiple sets to be identified using a regex pattern)foo");
}

void IdenticalProcessingSet::ProcessSets(
    AudioFiles &files,
    std::string_view command_name,
    const std::function<void(EditTrackedAudioFile *, const std::vector<EditTrackedAudioFile *> &)>
        &callback) {

    std::regex re {m_sample_set_args[0]};
    const auto &authority_matcher = m_sample_set_args[1];

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
            assert(std::regex_match(filename, match, re)); // TODO show an error
            assert(match.size() == 2); // TODO show an error
            if (match[1] == authority_matcher) {
                authority_file = f;
                break;
            }
        }

        if (authority_file) {
            MessageWithNewLine(
                command_name, {},
                "Processing sample-set \"{}\" (size {}) all with the same settings, using \"{}\" as the authority",
                human_set_name, set.second.size(), authority_matcher);
            callback(authority_file, set.second);
        } else {
            ErrorWithNewLine(
                command_name, {},
                "Failed to process sample-set because the authority file could not be identified\nFile: \"{}\"\nAuthority: \"{}\"",
                human_set_name, authority_matcher);
        }
    }
}

bool IdenticalProcessingSet::AllHaveSameNumFrames(const std::vector<EditTrackedAudioFile *> &set) {
    return std::all_of(set.begin(), set.end(), [&set](EditTrackedAudioFile *f) {
        return f->GetAudio().NumFrames() == set.front()->GetAudio().NumFrames();
    });
}
