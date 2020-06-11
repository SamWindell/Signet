#include "silence_remover.h"

#include "magic_enum.hpp"

#include "audio_file.h"
#include "common.h"
#include "test_helpers.h"
#include "types.h"

static constexpr usize silence_allowence = 4;

CLI::App *SilenceRemover::CreateSubcommandCLI(CLI::App &app) {
    auto silence_remover = app.add_subcommand(
        "silence-remove", "Silence-remover: removes silence from the start or end of the file(s). Silence is "
                          "considered anything under -90dB, "
                          "however this threshold can be changed with the --threshold option.");

    std::map<std::string, Region> region_name_dictionary;
    for (const auto e : magic_enum::enum_entries<Region>()) {
        region_name_dictionary[std::string(e.second)] = e.first;
    }
    silence_remover
        ->add_option("start-or-end", m_region,
                     "Specify whether the removal should be at the start, the end or both.")
        ->transform(CLI::CheckedTransformer(region_name_dictionary, CLI::ignore_case));

    silence_remover
        ->add_option("--threshold", m_silence_threshold_db,
                     "The threshold in decibels to which anything under it should be considered silence.")
        ->check(CLI::Range(-200, 0));
    return silence_remover;
}

bool SilenceRemover::ProcessAudio(AudioFile &input, const std::string_view filename) {
    if (input.interleaved_samples.size() == 0) return false;

    usize loud_region_start = 0;
    usize loud_region_end = input.NumFrames();
    const auto amp_threshold = DBToAmp(m_silence_threshold_db);

    if (m_region == Region::Start || m_region == Region::Both) {
        for (usize frame = 0; frame < input.NumFrames(); ++frame) {
            bool is_silent = true;
            for (unsigned channel = 0; channel < input.num_channels; ++channel) {
                if (std::abs(input.GetSample(channel, frame)) > amp_threshold) {
                    is_silent = false;
                    break;
                }
            }

            if (!is_silent) {
                loud_region_start = frame;
                break;
            }
        }
    }

    if (m_region == Region::End || m_region == Region::Both) {
        for (usize frame = input.NumFrames(); frame-- > 0;) {
            bool is_silent = true;
            for (unsigned channel = 0; channel < input.num_channels; ++channel) {
                if (std::abs(input.GetSample(channel, frame)) > amp_threshold) {
                    is_silent = false;
                    break;
                }
            }

            if (!is_silent) {
                loud_region_end = frame + 1;
                break;
            }
        }
    }

    // Allow there to be a few silent samples before we start chopping. Particularly at the start of a sample
    // it is often important to have some silent samples in order avoid pops at the start of playback.
    if (loud_region_start < silence_allowence)
        loud_region_start = 0;
    else
        loud_region_start -= silence_allowence;
    loud_region_end = std::min(input.NumFrames(), loud_region_end + 4);

    if (loud_region_start >= loud_region_end) {
        MessageWithNewLine("SilenceRemover", "The whole sample is silence - no change will be made");
        return false;
    }
    if (loud_region_start == 0 && loud_region_end == input.NumFrames()) {
        MessageWithNewLine("SilenceRemover", "No silence to trim at start or end");
        return false;
    }

    if (loud_region_start != 0 && loud_region_end != input.NumFrames()) {
        MessageWithNewLine("SilenceRemover", "Removing ", loud_region_start, " frames from the start and ",
                           input.NumFrames() - loud_region_end, " frames from the end");
    } else if (loud_region_start) {
        MessageWithNewLine("SilenceRemover", "Removing ", loud_region_start, " frames from the start");
    } else {
        MessageWithNewLine("SilenceRemover", "Removing ", input.NumFrames() - loud_region_end,
                           " frames from the end");
    }

    if (m_region == Region::End || m_region == Region::Both) {
        input.interleaved_samples.resize(loud_region_end * input.num_channels);
    }
    if (m_region == Region::Start || m_region == Region::Both) {
        input.interleaved_samples.erase(input.interleaved_samples.begin(),
                                        input.interleaved_samples.begin() +
                                            loud_region_start * input.num_channels);
    }

    return true;
}

TEST_CASE("[SilenceRemover]") {
    AudioFile buf;
    buf.num_channels = 1;
    buf.interleaved_samples.resize(silence_allowence * 2);
    buf.interleaved_samples.insert(buf.interleaved_samples.begin() + silence_allowence, {0.0, 0.0, 1.0, 0.0});
    const auto starting_size = buf.interleaved_samples.size();

    SUBCASE("no args") {
        REQUIRE_NOTHROW(TestHelpers::ProcessBufferWithSubcommand<SilenceRemover>("silence-remove", buf));
    }

    SUBCASE("all args") {
        REQUIRE_NOTHROW(TestHelpers::ProcessBufferWithSubcommand<SilenceRemover>(
            "silence-remove start --threshold -90", buf));
    }

    SUBCASE("just threshold") {
        REQUIRE_NOTHROW(
            TestHelpers::ProcessBufferWithSubcommand<SilenceRemover>("silence-remove --threshold -90", buf));
    }

    SUBCASE("removes start") {
        const auto result =
            TestHelpers::ProcessBufferWithSubcommand<SilenceRemover>("silence-remove start", buf);
        REQUIRE(result);
        REQUIRE(result->NumFrames() == starting_size - 2);
        REQUIRE(result->interleaved_samples[silence_allowence + 0] == 1.0);
        REQUIRE(result->interleaved_samples[silence_allowence + 1] == 0.0);
    }

    SUBCASE("removes end") {
        const auto result =
            TestHelpers::ProcessBufferWithSubcommand<SilenceRemover>("silence-remove end", buf);
        REQUIRE(result);
        REQUIRE(result->NumFrames() == starting_size - 1);
        REQUIRE(result->interleaved_samples[silence_allowence + 0] == 0.0);
        REQUIRE(result->interleaved_samples[silence_allowence + 1] == 0.0);
        REQUIRE(result->interleaved_samples[silence_allowence + 2] == 1.0);
    }

    SUBCASE("removes both") {
        const auto result =
            TestHelpers::ProcessBufferWithSubcommand<SilenceRemover>("silence-remove both", buf);
        REQUIRE(result);
        REQUIRE(result->NumFrames() == starting_size - 3);
        REQUIRE(result->interleaved_samples[silence_allowence + 0] == 1.0);
    }
}
