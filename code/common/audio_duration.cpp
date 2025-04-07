#include "audio_duration.h"

#include "doctest.hpp"

TEST_CASE("Audio Duration") {
    SUBCASE("validation") {
        REQUIRE_NOTHROW(AudioDuration("100smp"));
        REQUIRE_NOTHROW(AudioDuration("100s"));
        REQUIRE_NOTHROW(AudioDuration("100ms"));
        REQUIRE_NOTHROW(AudioDuration("100%"));
        REQUIRE_NOTHROW(AudioDuration("-10%"));
        REQUIRE_NOTHROW(AudioDuration("22.334%"));
        REQUIRE_THROWS(AudioDuration("foo"));

        REQUIRE(*AudioDuration::GetUnit("10") == AudioDuration::Unit::Samples);
        REQUIRE(*AudioDuration::GetUnit("10s") == AudioDuration::Unit::Seconds);
        REQUIRE(*AudioDuration::GetUnit("10ms") == AudioDuration::Unit::Milliseconds);
        REQUIRE(*AudioDuration::GetUnit("10smp") == AudioDuration::Unit::Samples);
        REQUIRE(*AudioDuration::GetUnit("10%") == AudioDuration::Unit::Percent);
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
