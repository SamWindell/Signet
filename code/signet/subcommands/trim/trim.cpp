#include "trim.h"

#include "audio_file_io.h"
#include "common.h"
#include "test_helpers.h"

CLI::App *Trimmer::CreateSubcommandCLI(CLI::App &app) {
    auto trim = app.add_subcommand(
        "trim",
        "Trimmer: removes the start or end of the file(s). This subcommand has itself 2 subcommands, 'start' "
        "and 'end'; one of which must be specified. For each, the amount to remove must be specified.");
    trim->require_subcommand();

    auto start = trim->add_subcommand("start", "Removes the start of the file.");
    start
        ->add_option("trim-start-length", m_start_duration,
                     "The amount to remove from the start. " + AudioDuration::TypeDescription())
        ->required();

    auto end = trim->add_subcommand("end", "Removes the end of the file.");
    end->add_option("trim-end-length", m_end_duration,
                    "The amount to remove from the end. " + AudioDuration::TypeDescription())
        ->required();

    return trim;
}

void Trimmer::ProcessFiles(AudioFiles &files) {
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
                GetName(),
                "The trim region would result in the whole sample being removed - no change will be made");
            continue;
        }

        if (m_start_duration && m_end_duration) {
            MessageWithNewLine(GetName(), "Trimming {} frames from the start and {} frames from the end",
                               remaining_region_start, audio.NumFrames() - remaining_region_end);
        } else if (m_start_duration) {
            MessageWithNewLine(GetName(), "Trimming {} frames from the start", remaining_region_start);
        } else {
            MessageWithNewLine(GetName(), "Trimming {} frames from the end",
                               audio.NumFrames() - remaining_region_end);
        }

        if (m_end_duration && remaining_region_end != audio.NumFrames()) {
            auto &out_audio = f.GetWritableAudio();
            out_audio.interleaved_samples.resize(remaining_region_end * out_audio.num_channels);
            out_audio.FramesWereRemovedFromEnd();
        }
        if (m_start_duration && remaining_region_start != 0) {
            auto &out_audio = f.GetWritableAudio();
            out_audio.interleaved_samples.erase(out_audio.interleaved_samples.begin(),
                                                out_audio.interleaved_samples.begin() +
                                                    remaining_region_start * out_audio.num_channels);
            out_audio.FramesWereRemovedFromStart(remaining_region_start);
        }
    }
}

TEST_CASE("[Trimmer]") {
    SUBCASE("single channel") {
        AudioData buf;
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
        AudioData buf;
        buf.num_channels = 2;
        buf.sample_rate = 44100;
        buf.interleaved_samples = {1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6};

        const auto result =
            TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim start 1smp end 2smp", buf);
        REQUIRE(result);
        REQUIRE(result->NumFrames() == 3);
        REQUIRE(result->interleaved_samples[0] == 2);
    }

    SUBCASE("handles metadata") {
        AudioData buf;
        buf.num_channels = 1;
        buf.sample_rate = 48000;
        buf.interleaved_samples.resize(10);

        SUBCASE("marker") {
            SUBCASE("marker is removed if the marker is in the region that was removed") {
                SUBCASE("start") {
                    buf.metadata.markers.push_back({"marker1", 0});
                    const auto out =
                        TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim start 1smp", buf);
                    REQUIRE(out);
                    REQUIRE(out->metadata.markers.size() == 0);
                }
                SUBCASE("end") {
                    buf.metadata.markers.push_back({"marker1", 9});
                    const auto out = TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim end 1smp", buf);
                    REQUIRE(out);
                    REQUIRE(out->metadata.markers.size() == 0);
                }
            }
            SUBCASE("marker position moves if start trimmed") {
                buf.metadata.markers.push_back({"marker1", 2});
                const auto out = TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim start 1smp", buf);
                REQUIRE(out);
                REQUIRE(out->metadata.markers.size() == 1);
                REQUIRE(out->metadata.markers[0].start_frame == 1);
            }
        }

        SUBCASE("region") {
            SUBCASE("region is removed if the region is in the bit that was removed") {
                SUBCASE("start") {
                    buf.metadata.regions.push_back({"region marker", "region name", 0, 2});
                    const auto out =
                        TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim start 1smp", buf);
                    REQUIRE(out);
                    REQUIRE(out->metadata.regions.size() == 0);
                }
                SUBCASE("end") {
                    buf.metadata.regions.push_back({"region marker", "region name", 8, 2});
                    const auto out = TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim end 1smp", buf);
                    REQUIRE(out);
                    REQUIRE(out->metadata.regions.size() == 0);
                }
            }
            SUBCASE("region position moves if start trimmed") {
                buf.metadata.regions.push_back({"region marker", "region name", 2, 2});
                const auto out = TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim start 1smp", buf);
                REQUIRE(out);
                REQUIRE(out->metadata.regions.size() == 1);
                REQUIRE(out->metadata.regions[0].start_frame == 1);
            }
        }

        SUBCASE("loop") {
            SUBCASE("loop is removed if the loop is in the bit that was removed") {
                SUBCASE("start") {
                    buf.metadata.loops.push_back({"loop name", MetadataItems::LoopType::Forward, 0, 2, 0});
                    const auto out =
                        TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim start 1smp", buf);
                    REQUIRE(out);
                    REQUIRE(out->metadata.loops.size() == 0);
                }
                SUBCASE("end") {
                    buf.metadata.loops.push_back({"loop name", MetadataItems::LoopType::Forward, 8, 2, 0});
                    const auto out = TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim end 1smp", buf);
                    REQUIRE(out);
                    REQUIRE(out->metadata.loops.size() == 0);
                }
            }
            SUBCASE("loop position moves if start trimmed") {
                buf.metadata.loops.push_back({"loop name", MetadataItems::LoopType::Forward, 2, 2, 0});
                const auto out = TestHelpers::ProcessBufferWithSubcommand<Trimmer>("trim start 1smp", buf);
                REQUIRE(out);
                REQUIRE(out->metadata.loops.size() == 1);
                REQUIRE(out->metadata.loops[0].start_frame == 1);
            }
        }
    }
}
