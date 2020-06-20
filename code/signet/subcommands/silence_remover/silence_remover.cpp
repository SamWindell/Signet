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

void SilenceRemover::ProcessFiles(const tcb::span<EditTrackedAudioFile> files) {
    for (auto &f : files) {
        auto &audio = f.GetAudio();
        usize loud_region_start = 0;
        usize loud_region_end = audio.NumFrames();
        const auto amp_threshold = DBToAmp(m_silence_threshold_db);

        if (m_region == Region::Start || m_region == Region::Both) {
            for (usize frame = 0; frame < audio.NumFrames(); ++frame) {
                bool is_silent = true;
                for (unsigned channel = 0; channel < audio.num_channels; ++channel) {
                    if (std::abs(audio.GetSample(channel, frame)) > amp_threshold) {
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
            for (usize frame = audio.NumFrames(); frame-- > 0;) {
                bool is_silent = true;
                for (unsigned channel = 0; channel < audio.num_channels; ++channel) {
                    if (std::abs(audio.GetSample(channel, frame)) > amp_threshold) {
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

        // Allow there to be a few silent samples before we start chopping. Particularly at the start of a
        // sample it is often important to have some silent samples in order avoid pops at the start of
        // playback.
        if (loud_region_start < silence_allowence)
            loud_region_start = 0;
        else
            loud_region_start -= silence_allowence;
        loud_region_end = std::min(audio.NumFrames(), loud_region_end + 4);

        if (loud_region_start >= loud_region_end) {
            MessageWithNewLine("SilenceRemover", "The whole sample is silence - no change will be made");
            continue;
        }
        if (loud_region_start == 0 && loud_region_end == audio.NumFrames()) {
            MessageWithNewLine("SilenceRemover", "No silence to trim at start or end");
            continue;
        }

        if (loud_region_start != 0 && loud_region_end != audio.NumFrames()) {
            MessageWithNewLine("SilenceRemover", "Removing ", loud_region_start,
                               " frames from the start and ", audio.NumFrames() - loud_region_end,
                               " frames from the end");
        } else if (loud_region_start) {
            MessageWithNewLine("SilenceRemover", "Removing ", loud_region_start, " frames from the start");
        } else {
            MessageWithNewLine("SilenceRemover", "Removing ", audio.NumFrames() - loud_region_end,
                               " frames from the end");
        }

        if (m_region == Region::End || m_region == Region::Both) {
            const auto new_size = loud_region_end * audio.num_channels;
            if (!audio.interleaved_samples.size() != new_size) {
                f.GetWritableAudio().interleaved_samples.resize(new_size);
            }
        }
        if (m_region == Region::Start || m_region == Region::Both) {
            const auto num_samples = loud_region_start * audio.num_channels;
            if (num_samples) {
                auto &out_audio = f.GetWritableAudio();
                out_audio.interleaved_samples.erase(out_audio.interleaved_samples.begin(),
                                                    out_audio.interleaved_samples.begin() + num_samples);
            }
        }
    }
}

TEST_CASE("[SilenceRemover]") {
    AudioData buf;
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
