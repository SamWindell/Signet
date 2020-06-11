#include "trimmer.h"

#include "audio_file.h"
#include "common.h"
#include "test_helpers.h"

CLI::App *Trimmer::CreateSubcommandCLI(CLI::App &app) {
    auto trimmer = app.add_subcommand("trim", "Remove the start or end of the audio file");
    trimmer->require_subcommand();

    auto start = trimmer->add_subcommand("start", "Remove the start of the file");
    start
        ->add_option("trim-start-length", m_start_duration,
                     WrapText("The amount to remove from the start. " + AudioDuration::TypeDescription(), 80))
        ->required();

    auto end = trimmer->add_subcommand("end", "Remove the end of the file");
    end->add_option("trim-end-length", m_end_duration,
                    WrapText("The amount to remove from the end. " + AudioDuration::TypeDescription(), 80))
        ->required();

    return trimmer;
}

bool Trimmer::ProcessAudio(AudioFile &input, const std::string_view filename) {
    if (input.interleaved_samples.size() == 0) return false;

    usize remaining_region_start = 0, remaining_region_end = input.NumFrames();
    if (m_start_duration) {
        const auto start_size = m_start_duration->GetDurationAsFrames(input.sample_rate, input.NumFrames());
        remaining_region_start = start_size;
    }
    if (m_end_duration) {
        const auto end_size = m_end_duration->GetDurationAsFrames(input.sample_rate, input.NumFrames());
        remaining_region_end = input.NumFrames() - end_size;
    }

    if (remaining_region_start >= remaining_region_end) {
        WarningWithNewLine(
            "The trim region would result in the whole sample being removed - no change will be made");
        return false;
    }

    if (m_start_duration && m_end_duration) {
        MessageWithNewLine("Trimmer", "Trimming ", remaining_region_start, " frames from the start and ",
                           input.NumFrames() - remaining_region_end, " frames from the end");
    } else if (m_start_duration) {
        MessageWithNewLine("Trimmer", "Trimming ", remaining_region_start, " frames from the start");
    } else {
        MessageWithNewLine("Trimmer", "Trimming ", input.NumFrames() - remaining_region_end,
                           " frames from the end");
    }

    if (m_end_duration) {
        input.interleaved_samples.resize(remaining_region_end * input.num_channels);
    }
    if (m_start_duration) {
        input.interleaved_samples.erase(input.interleaved_samples.begin(),
                                        input.interleaved_samples.begin() +
                                            remaining_region_start * input.num_channels);
    }

    return true;
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
