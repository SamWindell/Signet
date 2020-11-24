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

int ScaleByOctavesToBeNearestToMiddleC(int midi_note) {
    constexpr int middle_c = 60;
    const auto pitch_index = midi_note % 12;
    if (pitch_index < 6) return middle_c + pitch_index;
    return middle_c - (12 - pitch_index);
}

TEST_CASE("Midi pitches") {
    REQUIRE(FindClosestMidiPitch(0).midi_note == 0);
    REQUIRE(FindClosestMidiPitch(440).midi_note == 69);
    REQUIRE(FindClosestMidiPitch(439).midi_note == 69);
    REQUIRE(FindClosestMidiPitch(441).midi_note == 69);
    REQUIRE(FindClosestMidiPitch(1566.55).midi_note == 91);
    REQUIRE(FindClosestMidiPitch(999999999).midi_note == 127);

    REQUIRE(ScaleByOctavesToBeNearestToMiddleC(1) == 61);
    REQUIRE(ScaleByOctavesToBeNearestToMiddleC(13) == 61);
    REQUIRE(ScaleByOctavesToBeNearestToMiddleC(25) == 61);
    REQUIRE(ScaleByOctavesToBeNearestToMiddleC(11) == 59);
}
