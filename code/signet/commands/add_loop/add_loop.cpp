#include "add_loop.h"

#include "CLI11.hpp"

#include "common.h"
#include "metadata.h"
#include "test_helpers.h"

CLI::App *AddLoopCommand::CreateCommandCLI(CLI::App &app) {
    auto add_loop = app.add_subcommand(
        "add-loop",
        "Adds a loop to the audio file(s). The loop is defined by a start point and either an end point or number of frames. These points can be negative, meaning they are relative to the end of the file.");

    add_loop
        ->add_option("start-point", m_start_point,
                     "The start point of the loop. " + AudioDuration::TypeDescription() +
                         " If negative, it's measured from the end of the file.")
        ->required();

    auto end_option = add_loop->add_option(
        "end-point", m_end_point,
        "The end point of the loop. " + AudioDuration::TypeDescription() +
            " If negative, it's measured from the end of the file. 0 means the end sample.");

    auto num_frames_option = add_loop->add_option(
        "--num-frames", m_num_frames,
        "Number of frames in the loop.  Can be used instead of specifying an end-point. " +
            AudioDuration::TypeDescription());

    end_option->excludes(num_frames_option);
    num_frames_option->excludes(end_option);

    add_loop->add_option("--name", m_loop_name, "Optional name for the loop.")->capture_default_str();

    std::map<std::string, MetadataItems::LoopType> loop_types;
    for (const auto &e : magic_enum::enum_entries<MetadataItems::LoopType>()) {
        loop_types[std::string(e.second)] = e.first;
    }

    add_loop->add_option("--type", m_loop_type, "Type of loop. Default is Forward.")
        ->transform(CLI::CheckedTransformer(loop_types, CLI::ignore_case))
        ->capture_default_str();

    add_loop
        ->add_option("--loop-count", m_num_times_to_loop,
                     "Number of times to loop. 0 means infinite looping (default).")
        ->capture_default_str();

    add_loop->callback([this]() {
        if (!m_end_point && !m_num_frames) {
            throw CLI::ValidationError("Must specify either end-point or --num-frames");
        }
    });

    return add_loop;
}

void AddLoopCommand::ProcessFiles(AudioFiles &files) {
    auto const start_is_relative = m_start_point.GetValue() < 0;

    // End point can be 0 which means the end of the file.
    auto const end_is_relative = m_end_point && m_end_point->GetValue() <= 0;

    for (auto &f : files) {
        auto &audio = f.GetAudio();
        if (audio.IsEmpty()) {
            WarningWithNewLine(GetName(), f, "File is empty, skipping");
            continue;
        }

        const size_t num_frames = audio.NumFrames();
        size_t start_frame = 0;
        size_t end_frame = 0;

        if (start_is_relative) {
            AudioDuration abs_start(m_start_point.GetUnit(), std::abs(m_start_point.GetValue()));
            size_t offset = abs_start.GetDurationAsFrames(audio.sample_rate, num_frames);
            start_frame = num_frames - offset;
        } else {
            start_frame = m_start_point.GetDurationAsFrames(audio.sample_rate, num_frames);
        }

        if (m_num_frames.has_value()) {
            size_t frames_to_add = m_num_frames->GetDurationAsFrames(audio.sample_rate, num_frames);
            end_frame = start_frame + frames_to_add;
        } else if (end_is_relative) {
            AudioDuration abs_end(m_end_point->GetUnit(), std::abs(m_end_point->GetValue()));
            size_t offset = abs_end.GetDurationAsFrames(audio.sample_rate, num_frames);
            end_frame = num_frames - offset;
        } else {
            end_frame = m_end_point->GetDurationAsFrames(audio.sample_rate, num_frames);
        }

        // Validate loop points
        if (start_frame >= end_frame) {
            ErrorWithNewLine(GetName(), f, "Invalid loop points: start ({}) must be before end ({})",
                             start_frame, end_frame);
            continue;
        }

        if (start_frame >= num_frames) {
            ErrorWithNewLine(GetName(), f, "Start point ({}) is beyond the end of the file ({})", start_frame,
                             num_frames);
            continue;
        }

        if (end_frame > num_frames) {
            ErrorWithNewLine(GetName(), f, "End point ({}) is beyond the end of the file ({})", end_frame,
                             num_frames);
            continue;
        }

        // Calculate loop length in frames
        size_t num_frames_in_loop = end_frame - start_frame;

        // Create the loop
        MetadataItems::Loop loop;
        loop.name = m_loop_name;
        loop.type = m_loop_type;
        loop.start_frame = start_frame;
        loop.num_frames = num_frames_in_loop;
        loop.num_times_to_loop = m_num_times_to_loop;

        // Add the loop to the file metadata
        auto &writable_audio = f.GetWritableAudio();
        writable_audio.metadata.loops.push_back(loop);

        // Also update the timing info to mark this as a looping file
        if (!writable_audio.metadata.timing_info) {
            // Create timing info if it doesn't exist
            MetadataItems::TimingInfo timing;
            timing.playback_type = MetadataItems::PlaybackType::Loop;
            writable_audio.metadata.timing_info = timing;
        } else {
            // Update existing timing info
            writable_audio.metadata.timing_info->playback_type = MetadataItems::PlaybackType::Loop;
        }

        std::string source_info = m_num_frames.has_value() ? "using --num-frames" : "using end-point";
        MessageWithNewLine(GetName(), f, "Added {} loop from frame {} to {} (duration: {} frames, {})",
                           magic_enum::enum_name(m_loop_type), start_frame, end_frame, num_frames_in_loop,
                           source_info);
    }
}

TEST_CASE("[AddLoopCommand]") {
    AudioData buf;
    buf.num_channels = 1;
    buf.sample_rate = 44100;
    buf.interleaved_samples.resize(44100); // 1 second of audio

    SUBCASE("Adds a loop with absolute positions") {
        const auto result =
            TestHelpers::ProcessBufferWithCommand<AddLoopCommand>("add-loop 0smp 44100smp", buf);
        REQUIRE(result);
        REQUIRE(result->metadata.loops.size() == 1);
        REQUIRE(result->metadata.loops[0].start_frame == 0);
        REQUIRE(result->metadata.loops[0].num_frames == 44100);
        REQUIRE(result->metadata.loops[0].type == MetadataItems::LoopType::Forward);
        REQUIRE(result->metadata.loops[0].num_times_to_loop == 0); // infinite
    }

    SUBCASE("Adds a whole-file loop") {
        const auto result = TestHelpers::ProcessBufferWithCommand<AddLoopCommand>("add-loop 0 0", buf);
        REQUIRE(result);
        REQUIRE(result->metadata.loops.size() == 1);
        REQUIRE(result->metadata.loops[0].start_frame == 0);
        REQUIRE(result->metadata.loops[0].num_frames == 44100);
        REQUIRE(result->metadata.loops[0].type == MetadataItems::LoopType::Forward);
        REQUIRE(result->metadata.loops[0].num_times_to_loop == 0); // infinite
    }

    SUBCASE("Adds a loop with end-relative end-point") {
        const auto result =
            TestHelpers::ProcessBufferWithCommand<AddLoopCommand>("add-loop 0smp -10smp", buf);
        REQUIRE(result);
        REQUIRE(result->metadata.loops.size() == 1);
        REQUIRE(result->metadata.loops[0].start_frame == 0);
        REQUIRE(result->metadata.loops[0].num_frames == 44090);
        REQUIRE(result->metadata.loops[0].type == MetadataItems::LoopType::Forward);
        REQUIRE(result->metadata.loops[0].num_times_to_loop == 0); // infinite
    }

    SUBCASE("Adds a loop with a time-based end-relative end-point") {
        const auto result = TestHelpers::ProcessBufferWithCommand<AddLoopCommand>("add-loop 0 -0.25s", buf);
        REQUIRE(result);
        REQUIRE(result->metadata.loops.size() == 1);
        REQUIRE(result->metadata.loops[0].start_frame == 0);
        REQUIRE(result->metadata.loops[0].num_frames == 33075);
        REQUIRE(result->metadata.loops[0].type == MetadataItems::LoopType::Forward);
        REQUIRE(result->metadata.loops[0].num_times_to_loop == 0); // infinite
    }

    SUBCASE("Adds a loop with num-frames instead of end-point") {
        const auto result =
            TestHelpers::ProcessBufferWithCommand<AddLoopCommand>("add-loop 0smp --num-frames 44100smp", buf);
        REQUIRE(result);
        REQUIRE(result->metadata.loops.size() == 1);
        REQUIRE(result->metadata.loops[0].start_frame == 0);
        REQUIRE(result->metadata.loops[0].num_frames == 44100);
        REQUIRE(result->metadata.loops[0].type == MetadataItems::LoopType::Forward);
        REQUIRE(result->metadata.loops[0].num_times_to_loop == 0); // infinite
    }

    SUBCASE("Adds a loop with time-based positions") {
        const auto result =
            TestHelpers::ProcessBufferWithCommand<AddLoopCommand>("add-loop 0.25s 0.75s", buf);
        REQUIRE(result);
        REQUIRE(result->metadata.loops.size() == 1);
        REQUIRE(result->metadata.loops[0].start_frame == 11025); // 0.25 * 44100
        REQUIRE(result->metadata.loops[0].num_frames == 22050); // (0.75 - 0.25) * 44100
    }

    SUBCASE("Adds a loop with percentage-based positions") {
        const auto result = TestHelpers::ProcessBufferWithCommand<AddLoopCommand>("add-loop 25% 75%", buf);
        REQUIRE(result);
        REQUIRE(result->metadata.loops.size() == 1);
        REQUIRE(result->metadata.loops[0].start_frame == 11025); // 0.25 * 44100
        REQUIRE(result->metadata.loops[0].num_frames == 22050); // (0.75 - 0.25) * 44100
    }

    SUBCASE("Handles custom loop type") {
        const auto result = TestHelpers::ProcessBufferWithCommand<AddLoopCommand>(
            "add-loop 0smp 44100smp --type pingpong", buf);
        REQUIRE(result);
        REQUIRE(result->metadata.loops.size() == 1);
        REQUIRE(result->metadata.loops[0].type == MetadataItems::LoopType::PingPong);
    }

    SUBCASE("Handles custom loop count") {
        const auto result = TestHelpers::ProcessBufferWithCommand<AddLoopCommand>(
            "add-loop 0smp 44100smp --loop-count 3", buf);
        REQUIRE(result);
        REQUIRE(result->metadata.loops.size() == 1);
        REQUIRE(result->metadata.loops[0].num_times_to_loop == 3);
    }

    SUBCASE("Handles custom loop name") {
        const auto result = TestHelpers::ProcessBufferWithCommand<AddLoopCommand>(
            "add-loop 0smp 44100smp --name TestLoop", buf);
        REQUIRE(result);
        REQUIRE(result->metadata.loops.size() == 1);
        REQUIRE(result->metadata.loops[0].name.has_value());
        REQUIRE(result->metadata.loops[0].name.value() == "TestLoop");
    }

    SUBCASE("Either end-point or num-frames must be specified") {
        REQUIRE_THROWS(TestHelpers::ProcessBufferWithCommand<AddLoopCommand>("add-loop 0smp", buf));
    }
}
