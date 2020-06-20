#include "trimmer.h"

#include "audio_file.h"
#include "common.h"
#include "test_helpers.h"

CLI::App *Trimmer::CreateSubcommandCLI(CLI::App &app) {
    auto trimmer = app.add_subcommand(
        "trim",
        "Trimmer: removes the start or end of the file(s). This subcommand has itself 2 subcommands, 'start' "
        "and 'end'; one of which must be specified. For each, the amount to remove must be specified.");
    trimmer->require_subcommand();

    auto start = trimmer->add_subcommand("start", "Removes the start of the file.");
    start
        ->add_option("trim-start-length", m_start_duration,
                     "The amount to remove from the start. " + AudioDuration::TypeDescription())
        ->required();

    auto end = trimmer->add_subcommand("end", "Removes the end of the file.");
    end->add_option("trim-end-length", m_end_duration,
                    "The amount to remove from the end. " + AudioDuration::TypeDescription())
        ->required();

    return trimmer;
}

void Trimmer::ProcessFiles(const tcb::span<InputAudioFile> files) {
    for (auto &f : files) {
        auto &audio = f.GetAudio();
        if (audio.IsEmpty()) continue;

        usize remaining_region_start = 0, remaining_region_end = audio.NumFrames();
        if (m_start_duration) {
            const auto start_size =
                m_start_duration->GetDurationAsFrames(audio.sample_rate, audio.NumFrames());
            remaining_region_start = start_size;
        }
        if (m_end_duration) {
            const auto end_size = m_end_duration->GetDurationAsFrames(audio.sample_rate, audio.NumFrames());
            remaining_region_end = audio.NumFrames() - end_size;
        }

        if (remaining_region_start >= remaining_region_end) {
            WarningWithNewLine(
                "The trim region would result in the whole sample being removed - no change will be made");
            continue;
        }

        if (m_start_duration && m_end_duration) {
            MessageWithNewLine("Trimmer", "Trimming ", remaining_region_start, " frames from the start and ",
                               audio.NumFrames() - remaining_region_end, " frames from the end");
        } else if (m_start_duration) {
            MessageWithNewLine("Trimmer", "Trimming ", remaining_region_start, " frames from the start");
        } else {
            MessageWithNewLine("Trimmer", "Trimming ", audio.NumFrames() - remaining_region_end,
                               " frames from the end");
        }

        if (m_end_duration && remaining_region_end != audio.NumFrames()) {
            auto &out_audio = f.GetWritableAudio();
            out_audio.interleaved_samples.resize(remaining_region_end * out_audio.num_channels);
        }
        if (m_start_duration && remaining_region_start != 0) {
            auto &out_audio = f.GetWritableAudio();
            out_audio.interleaved_samples.erase(out_audio.interleaved_samples.begin(),
                                                out_audio.interleaved_samples.begin() +
                                                    remaining_region_start * out_audio.num_channels);
        }
    }
}

TEST_CASE("[Trimmer]") {
    SUBCASE("single channel") {
        AudioFile buf;
        buf.num_channels = 1;
        buf.sample_rate = 44100;
        buf.interleaved_samples = {1, 2, 3, 4, 5, 6};

        SUBCASE("both start and end") {
            REQUIRE_NOTHROW(TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim start 1s end 1s", buf));
        }

        SUBCASE("just start") {
            REQUIRE_NOTHROW(TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim start 1%", buf));
        }

        SUBCASE("just end") {
            REQUIRE_NOTHROW(TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim end 50smp", buf));
        }

        SUBCASE("neither start or end throws") {
            REQUIRE_THROWS(TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim", buf));
        }

        SUBCASE("trim start") {
            const auto result = TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim start 1smp", buf);
            REQUIRE(result);
            REQUIRE(result->interleaved_samples.size() == 5);
            REQUIRE(result->interleaved_samples[0] == 2);
        }

        SUBCASE("trim end") {
            const auto result = TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim end 1smp", buf);
            REQUIRE(result);
            REQUIRE(result->interleaved_samples.size() == 5);
            REQUIRE(result->interleaved_samples[0] == 1);
        }
    }

    SUBCASE("multiple channels") {
        AudioFile buf;
        buf.num_channels = 2;
        buf.sample_rate = 44100;
        buf.interleaved_samples = {1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6};

        const auto result =
            TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim start 1smp end 2smp", buf);
        REQUIRE(result);
        REQUIRE(result->NumFrames() == 3);
        REQUIRE(result->interleaved_samples[0] == 2);
    }
}
