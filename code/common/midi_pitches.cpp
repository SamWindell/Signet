#include "midi_pitches.h"

#include <cfloat>
#include <cmath>

#include "doctest.hpp"

MIDIPitch FindClosestMidiPitch(const double freq) {
    double min_distance = DBL_MAX;
    size_t min_index = (size_t)-1;
    for (size_t i = 0; i < std::size(g_midi_pitches); ++i) {
        const double delta = std::abs(freq - g_midi_pitches[i].pitch);
        if (delta < min_distance) {
            min_distance = delta;
            min_index = i;
        }
    }
    REQUIRE(min_index != (size_t)-1);

    return g_midi_pitches[min_index];
}

TEST_CASE("Midi pitches") {
    REQUIRE(FindClosestMidiPitch(0).midi_note == 0);
    REQUIRE(FindClosestMidiPitch(440).midi_note == 69);
    REQUIRE(FindClosestMidiPitch(439).midi_note == 69);
    REQUIRE(FindClosestMidiPitch(441).midi_note == 69);
    REQUIRE(FindClosestMidiPitch(999999999).midi_note == 127);
}
