#pragma once
#include <string_view>

namespace RenameSubstitution {

struct Variable {
    std::string_view name;
    std::string_view desc;
};

static const Variable g_vars[] = {
    {"<counter>", "A unique number starting from zero. The ordering of these numbers is not specified."},

    {"<alpha-counter>",
     "A unique 3 character counter starting from aaa and ending with zzz. Beyond zzz, <alpha-counter> will "
     "be replaced with a number instead. The ordering of these numbers is not specified."},

    {"<detected-pitch>",
     "The detected pitch of audio file in Hz. If no pitch is found this variable will be empty."},

    {"<detected-midi-note>", "The MIDI note number that is closest to the detected pitch of the audio file. "
                             "If no pitch is found this variable will be empty."},

    {"<detected-midi-note-octave-plus-1>",
     "The MIDI note number (+12 semitones) that is closest to the detected pitch of the audio file. If no "
     "pitch is found this variable will be empty."},

    {"<detected-midi-note-octave-plus-2>",
     "The MIDI note number (+24 semitones) that is closest to the detected pitch of the audio file. If no "
     "pitch is found this variable will be empty."},

    {"<detected-midi-note-octave-minus-1>",
     "The MIDI note number (-12 semitones) that is closest to the detected pitch of the audio file. If no "
     "pitch is found this variable will be empty."},

    {"<detected-midi-note-octave-minus-2>",
     "The MIDI note number (-24 semitones) that is closest to the detected pitch of the audio file. If no "
     "pitch is found this variable will be empty."},

    {"<detected-note>", "The musical note-name that is closest to the detected pitch of the audio file. The "
                        "note is capitalised, and the octave number is specified. For example 'C3'. If no "
                        "pitch is found this variable will be empty."},

    {"<parent-folder>", "The name of the folder that contains the audio file."},

    {"<parent-folder-snake>", "The snake-case name of the folder that contains the audio file."},

    {"<parent-folder-camel>", "The camel-case name of the folder that contains the audio file."},
};

std::string GetFullInfo();
std::string GetVariableNames();

} // namespace RenameSubstitution
