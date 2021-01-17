#include "remove_silence.h"

#include "magic_enum.hpp"

#include "audio_file_io.h"
#include "common.h"
#include "test_helpers.h"
#include "types.h"

static constexpr usize silence_allowence = 4;

CLI::App *SilenceRemover::CreateSubcommandCLI(CLI::App &app) {
    auto remove_silence = app.add_subcommand(
        "remove-silence", "Silence-remover: removes silence from the start or end of the file(s). Silence is "
                          "considered anything under -90dB, "
                          "however this threshold can be changed with the --threshold option.");

    std::map<std::string, Region> region_name_dictionary;
    for (const auto &e : magic_enum::enum_entries<Region>()) {
        region_name_dictionary[std::string(e.second)] = e.first;
    }
    remove_silence
        ->add_option("start-or-end", m_region,
                     "Specify whether the removal should be at the start, the end or both.")
        ->transform(CLI::CheckedTransformer(region_name_dictionary, CLI::ignore_case));

    remove_silence
        ->add_option("--threshold", m_silence_threshold_db,
                     "The threshold in decibels to which anything under it should be considered silence.")
        ->check(CLI::Range(-200, 0));
    return remove_silence;
}

void SilenceRemover::ProcessFiles(AudioFiles &files) {
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
            MessageWithNewLine(GetName(), "The whole sample is silence - no change will be made");
            continue;
        }
        if (loud_region_start == 0 && loud_region_end == audio.NumFrames()) {
            MessageWithNewLine(GetName(), "No silence to trim at start or end");
            continue;
        }

        if (loud_region_start != 0 && loud_region_end != audio.NumFrames()) {
            MessageWithNewLine(GetName(), "Removing {} frames from the start and {} frames from the end",
                               loud_region_start, audio.NumFrames() - loud_region_end);
        } else if (loud_region_start) {
            MessageWithNewLine(GetName(), "Removing {} frames from the start", loud_region_start);
        } else {
            MessageWithNewLine(GetName(), "Removing {} frames from the end",
                               audio.NumFrames() - loud_region_end);
        }

        if (m_region == Region::End || m_region == Region::Both) {
            const auto new_size = loud_region_end * audio.num_channels;
            if (audio.interleaved_samples.size() != new_size) {
                f.GetWritableAudio().interleaved_samples.resize(new_size);
                f.GetWritableAudio().FramesWereRemovedFromEnd();
            }
        }
        if (m_region == Region::Start || m_region == Region::Both) {
            const auto num_samples = loud_region_start * audio.num_channels;
            if (num_samples) {
                auto &out_audio = f.GetWritableAudio();
                out_audio.interleaved_samples.erase(out_audio.interleaved_samples.begin(),
                                                    out_audio.interleaved_samples.begin() + num_samples);
                out_audio.FramesWereRemovedFromStart(loud_region_start);
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
        REQUIRE_NOTHROW(TestHelpers::ProcessBufferWithSubcommand<SilenceRemover>("remove-silence", buf));
    }

    SUBCASE("all args") {
        REQUIRE_NOTHROW(TestHelpers::ProcessBufferWithSubcommand<SilenceRemover>(
            "remove-silence start --threshold -90", buf));
    }

    SUBCASE("just threshold") {
        REQUIRE_NOTHROW(
            TestHelpers::ProcessBufferWithSubcommand<SilenceRemover>("remove-silence --threshold -90", buf));
    }

    SUBCASE("removes start") {
        const auto result =
            TestHelpers::ProcessBufferWithSubcommand<SilenceRemover>("remove-silence start", buf);
        REQUIRE(result);
        REQUIRE(result->NumFrames() == starting_size - 2);
        REQUIRE(result->interleaved_samples[silence_allowence + 0] == 1.0);
        REQUIRE(result->interleaved_samples[silence_allowence + 1] == 0.0);
    }

    SUBCASE("removes end") {
        const auto result =
            TestHelpers::ProcessBufferWithSubcommand<SilenceRemover>("remove-silence end", buf);
        REQUIRE(result);
        REQUIRE(result->NumFrames() == starting_size - 1);
        REQUIRE(result->interleaved_samples[silence_allowence + 0] == 0.0);
        REQUIRE(result->interleaved_samples[silence_allowence + 1] == 0.0);
        REQUIRE(result->interleaved_samples[silence_allowence + 2] == 1.0);
    }

    SUBCASE("removes both") {
        const auto result =
            TestHelpers::ProcessBufferWithSubcommand<SilenceRemover>("remove-silence both", buf);
        REQUIRE(result);
        REQUIRE(result->NumFrames() == starting_size - 3);
        REQUIRE(result->interleaved_samples[silence_allowence + 0] == 1.0);
    }
}
