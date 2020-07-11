#include "note_to_midi.h"

void NoteToMIDIConverter::CreateCLI(CLI::App &renamer) {
    auto note_to_midi = renamer.add_subcommand(
        "note-to-midi", "Replace all occurrences of note names with the corresponding MIDI note number. For "
                        "example replace C3 with 60.");
    note_to_midi->final_callback([this] { m_on = true; });
    note_to_midi->add_option("--midi-zero-note", m_midi_0_note,
                             "The note that should represent MIDI note number 0. Default is C-1.");
}

static const std::array<std::string_view, 12> g_notes {"c",  "c#", "d",  "d#", "e",  "f",
                                                       "f#", "g",  "g#", "a",  "a#", "b"};

struct Note {
    std::string_view note_letter;
    int note_index;
    int octave;
};

static int SemitoneDistance(const Note &a, const Note &b) {
    const auto octave_distance = b.octave - a.octave;
    const auto note_distance = b.note_index - a.note_index;
    return octave_distance * 12 + note_distance;
}

static std::optional<Note> ParseNote(std::string note_string) {
    Lowercase(note_string);
    Note note {};
    for (usize i = 0; i < g_notes.size(); ++i) {
        if (StartsWith(note_string, g_notes[i])) {
            note.note_index = (int)i;
            note.note_letter = g_notes[i];
        }
    }
    if (note.note_letter.size() == 0) {
        return {};
    }
    try {
        note.octave = std::stoi(note_string.substr(note.note_letter.size()));
    } catch (...) {
        return {};
    }
    return note;
}

static std::optional<std::string_view> FindNoteName(std::string_view str) {
    for (usize i = 0; i < str.size(); ++i) {
        auto pos = i;
        if (pos == 0 || !std::isalnum(str[pos])) {
            if (pos != 0) pos++;
            if (pos < str.size()) {
                if ((str[pos] >= 'a' && str[pos] <= 'g') || (str[pos] >= 'A' && str[pos] <= 'G')) {
                    const auto start_index = pos;
                    pos++;
                    if (pos < str.size()) {
                        if (str[pos] == '#') {
                            pos++;
                        }
                        if (pos < str.size()) {
                            if (str[pos] == '-') {
                                pos++;
                            }
                            if (pos < str.size()) {
                                if (std::isdigit(str[pos])) {
                                    pos++;
                                    const auto end_index = pos;
                                    if (pos == str.size() || !std::isalnum(str[pos])) {
                                        return str.substr(start_index, end_index - start_index);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return {};
}

bool NoteToMIDIConverter::Rename(const EditTrackedAudioFile &f, std::string &filename) {
    if (m_on) {
        const auto zero_note = ParseNote(m_midi_0_note);
        if (!zero_note) {
            WarningWithNewLine("Renamer note-to-midi: given root note is not valid: ", m_midi_0_note);
        } else {
            bool result = false;
            while (const auto note_str = FindNoteName(filename)) {
                const auto note = ParseNote(std::string(*note_str));
                if (!note) continue;
                const auto replacement = std::to_string(SemitoneDistance(*zero_note, *note));
                Replace(filename, std::string(*note_str), replacement);
                result = true;
            }
            return result;
        }
    }
    return false;
}

TEST_CASE("note functions") {
    SUBCASE("parse from string") {
        SUBCASE("lower") {
            const auto r = ParseNote("c1");
            REQUIRE(r);
            REQUIRE(r->note_letter == "c");
            REQUIRE(r->note_index == 0);
            REQUIRE(r->octave == 1);
        }
        SUBCASE("upper sharp negative octave") {
            const auto r = ParseNote("G#-1");
            REQUIRE(r);
            REQUIRE(r->note_letter == "g#");
            REQUIRE(r->note_index == 8);
            REQUIRE(r->octave == -1);
        }
        SUBCASE("fail with incorrect note") { REQUIRE(!ParseNote("z0")); }
        SUBCASE("fail with missing octave") { REQUIRE(!ParseNote("c")); }
        SUBCASE("fail with empty string") { REQUIRE(!ParseNote("")); }
    }
    SUBCASE("distance") {
        auto GetDistance = [&](auto a, auto b) { return SemitoneDistance(*ParseNote(a), *ParseNote(b)); };
        REQUIRE(GetDistance("C0", "C#0") == 1);
        REQUIRE(GetDistance("C#0", "C0") == -1);
        REQUIRE(GetDistance("C0", "C1") == 12);
        REQUIRE(GetDistance("C1", "C0") == -12);
        REQUIRE(GetDistance("C1", "B0") == -1);
        REQUIRE(GetDistance("B0", "C1") == 1);
        REQUIRE(GetDistance("B-2", "B2") == 48);
    }
    SUBCASE("find note name") {
        auto Exists = [&](std::string_view haystack, std::string_view note_name) {
            const auto r = FindNoteName(haystack);
            return r && *r == note_name;
        };

        REQUIRE(Exists("file_c-1.wav", "c-1"));
        REQUIRE(Exists("file_c1.wav", "c1"));
        REQUIRE(Exists("file_C1.wav", "C1"));
        REQUIRE(Exists("file_g#2.wav", "g#2"));
        REQUIRE(Exists("file_g-2.wav", "g-2"));
        REQUIRE(Exists("file_g#-2.wav", "g#-2"));
        REQUIRE(Exists("file_c1", "c1"));
        REQUIRE(Exists("file_C1", "C1"));
        REQUIRE(Exists("file_g#2", "g#2"));
        REQUIRE(Exists("file_g-2", "g-2"));
        REQUIRE(Exists("file_g#-2", "g#-2"));
        REQUIRE(Exists("c1-g", "c1"));
        REQUIRE(Exists("C#-2", "C#-2"));
        REQUIRE(!Exists("music1", ""));
        REQUIRE(!Exists("c1333", ""));
        REQUIRE(!Exists("c#1333", ""));
        REQUIRE(!Exists("c#-1333", ""));
    }
}
