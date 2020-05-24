#include "audio_duration.h"

#include "doctest.hpp"

TEST_CASE("Audio Duration") {
    SUBCASE("validation") {
        REQUIRE(AudioDuration::ValidateString("100smp") == "");
        REQUIRE(AudioDuration::ValidateString("100s") == "");
        REQUIRE(AudioDuration::ValidateString("100ms") == "");
        REQUIRE(AudioDuration::ValidateString("100%") == "");
        REQUIRE(AudioDuration::ValidateString("22.334%") == "");
        REQUIRE(AudioDuration::ValidateString("foo") != "");
        REQUIRE(AudioDuration::ValidateString("10") != "");
    }

    SUBCASE("constructors") {
        AudioDuration value_init {AudioDuration::Unit::Seconds, 100};
        AudioDuration string_init {"100s"};
        REQUIRE(value_init == string_init);
    }

    SUBCASE("values") {
        SUBCASE("samples") {
            AudioDuration a {AudioDuration::Unit::Samples, 10};
            REQUIRE(a.GetDurationAsFrames(44100, 100) == 10);
        }

        SUBCASE("seconds") {
            AudioDuration a {AudioDuration::Unit::Seconds, 1};
            REQUIRE(a.GetDurationAsFrames(44100, 44100) == 44100);
        }

        SUBCASE("milliseconds") {
            AudioDuration a {AudioDuration::Unit::Milliseconds, 1000};
            REQUIRE(a.GetDurationAsFrames(44100, 44100) == 44100);
        }

        SUBCASE("percent") {
            AudioDuration a {AudioDuration::Unit::Percent, 10};
            REQUIRE(a.GetDurationAsFrames(44100, 100) == 10);
        }
    }
}
