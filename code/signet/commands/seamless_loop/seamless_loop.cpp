#include "seamless_loop.h"

#include "CLI11.hpp"

#include "commands/fade/fade.h"
#include "commands/zcross_offset/zcross_offset.h"
#include "test_helpers.h"
#include "tests_config.h"

CLI::App *SeamlessLoopCommand::CreateCommandCLI(CLI::App &app) {
    auto looper = app.add_subcommand(
        "seamless-loop",
        "Turns the file(s) into seamless loops. If you specify a crossfade-percent of 0, the algorithm will trim the file down to the smallest possible seamless-sounding loop, which starts and ends on a zero crossings. If you specify a non-zero crossfade-percent, the given percentage of audio from the start of the file will be faded onto the end of the file. Due to this overlap, the resulting file is shorter.");
    looper
        ->add_option(
            "crossfade-percent", m_crossfade_percent,
            "The size of the crossfade region as a percent of the whole file. If this is zero then the algorithm will scan the file for the smallest possible loop, starting and ending on zero-crossings, and trim the file to that be that loop.")
        ->required()
        ->check(CLI::Range(0, 100));
    return looper;
}

void SeamlessLoopCommand::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        const auto num_frames = f.GetAudio().NumFrames();

        if (m_crossfade_percent != 0) {
            const auto num_xfade_frames = usize(num_frames * (m_crossfade_percent / 100.0));
            if (num_frames < num_xfade_frames || num_xfade_frames == 0) {
                ErrorWithNewLine(
                    GetName(), f,
                    "Cannot make the file a seamless loop because the file or crossfade-region are too small. Number of frames in the file: {}, number of frames in the crossfade-region: {}",
                    num_frames, num_xfade_frames);
                continue;
            }
            auto &audio = f.GetWritableAudio();
            FadeCommand::PerformFade(audio, 0, num_xfade_frames, FadeCommand::Shape::Sine);
            FadeCommand::PerformFade(audio, num_frames - 1, (num_frames - 1) - num_xfade_frames,
                                     FadeCommand::Shape::Sine);
            for (usize i = 0; i < num_xfade_frames; ++i) {
                auto write_index = (num_frames - 1) - (num_xfade_frames - 1) + i;
                for (unsigned chan = 0; chan < audio.num_channels; ++chan) {
                    audio.GetSample(chan, write_index) += audio.GetSample(chan, i);
                }
            }
            auto &samples = audio.interleaved_samples;
            samples.erase(samples.begin(), samples.begin() + num_xfade_frames * audio.num_channels);
            audio.FramesWereRemovedFromStart(num_xfade_frames);
        } else {
            auto &audio = f.GetAudio();

            // With this algorithm, we scan the file in small chunks. We do this using 2 scan windows. In each
            // window we find the best zero-crossing and then check how the audio will play if it were to
            // seamlessly loop from these 2 zero-crossings.
            constexpr double chunk_length_ms = 60;
            const size_t chunk_size = (size_t)(audio.sample_rate * (chunk_length_ms / 1000.0));

            // The scan the after each of the zero-crossings that we find. Here we specify the length of that
            // scan. See the "Stage 2" comment below for more info.
            constexpr double similarity_scan_length_ms = 35;
            const size_t similarity_scan_length_frames =
                (size_t)(audio.sample_rate * (similarity_scan_length_ms / 1000.0));

            if (chunk_size > audio.NumFrames())
                WarningWithNewLine(GetName(), f.GetPath(), "File is too short to process");

            bool performed_seamless_loop = false;

            struct Match {
                double percent_match;
                size_t start_frame, end_frame;
            };
            std::vector<Match> matches;

            for (size_t pos = 0; pos < audio.NumFrames(); pos += chunk_size) {
                const auto start_region =
                    tcb::span<const double> {audio.interleaved_samples}.subspan(pos * audio.num_channels);
                const auto start_zcross =
                    ZeroCrossOffsetCommand::FindFrameNearestToZeroInBuffer(
                        start_region, std::min(audio.NumFrames() - pos, chunk_size), audio.num_channels) +
                    pos;
                assert(ApproxEqual(audio.interleaved_samples[start_zcross * audio.num_channels], 0, 0.2));

                for (size_t end_pos = pos + chunk_size; end_pos < audio.NumFrames(); end_pos += chunk_size) {
                    const auto end_region = tcb::span<const double> {audio.interleaved_samples}.subspan(
                        end_pos * audio.num_channels);
                    const auto end_zcross = ZeroCrossOffsetCommand::FindFrameNearestToZeroInBuffer(
                                                end_region, std::min(audio.NumFrames() - end_pos, chunk_size),
                                                audio.num_channels) +
                                            end_pos;
                    assert(ApproxEqual(audio.interleaved_samples[end_zcross * audio.num_channels], 0, 0.2));

                    if ((start_zcross + similarity_scan_length_frames) > end_zcross) continue;
                    if ((audio.NumFrames() - end_pos) < similarity_scan_length_frames) continue;

                    // Stage 1: We check a small number of frames at the start/end. When the sample is played
                    // as a seamless loop these will be the first samples that make up the transition.
                    // Therefore it is important that the match is really strong. So here we check for a
                    // strong match, and if not, we bail.

                    constexpr double short_similarity_scan_frames = 20;
                    assert(short_similarity_scan_frames <= similarity_scan_length_frames);

                    bool loop_point_is_incredibly_similar = true;
                    for (size_t i = 0; i < short_similarity_scan_frames * audio.num_channels; ++i) {
                        auto &samples = audio.interleaved_samples;
                        if (!ApproxEqual(samples[start_zcross * audio.num_channels + i],
                                         samples[end_zcross * audio.num_channels + i], 0.05)) {
                            loop_point_is_incredibly_similar = false;
                            break;
                        }
                    }

                    if (!loop_point_is_incredibly_similar) break;

                    // Stage 2: We check a longer region for similarity. We give the strength of the match a
                    // percentage and store it in a buffer, so that later on we can pick the region that has
                    // the greatest match.

                    size_t num_samples_equal = 0;
                    for (size_t i = 0; i < similarity_scan_length_frames * audio.num_channels; ++i) {
                        auto &samples = audio.interleaved_samples;
                        if (ApproxEqual(samples[start_zcross * audio.num_channels + i],
                                        samples[end_zcross * audio.num_channels + i], 0.2))
                            ++num_samples_equal;
                    }
                    const double num_frames_equal = (double)num_samples_equal / audio.num_channels;

                    const auto percent_equal =
                        (num_frames_equal / (double)similarity_scan_length_frames) * 100.0;
                    matches.push_back({percent_equal, start_zcross, end_zcross});
                }
            }

            std::sort(matches.begin(), matches.end(),
                      [](const Match &a, const Match &b) { return a.percent_match < b.percent_match; });

            if (!matches.size() || matches[0].percent_match < 60)
                WarningWithNewLine(GetName(), f.GetPath(), "Failed to find a seamless loop");

            MessageWithNewLine(GetName(), f.GetPath(), "Found a seamless loop of length {:.2f} seconds",
                               (double)(matches[0].end_frame - matches[0].start_frame) /
                                   (double)audio.sample_rate);

            auto &writable_audio = f.GetWritableAudio();
            auto &samples = writable_audio.interleaved_samples;
            auto best_match = matches[0];

            assert(ApproxEqual(samples[best_match.start_frame * audio.num_channels], 0, 0.2));
            assert(ApproxEqual(samples[best_match.end_frame * audio.num_channels], 0, 0.2));

            samples.erase(samples.begin(), samples.begin() + best_match.start_frame * audio.num_channels);
            writable_audio.FramesWereRemovedFromStart(best_match.start_frame);

            samples.erase(samples.begin() +
                              (best_match.end_frame - best_match.start_frame) * audio.num_channels,
                          samples.end());
            writable_audio.FramesWereRemovedFromEnd();
        }
    }
}

TEST_CASE("SeamlessLoopCommand") {
    SUBCASE("crossfade") {
        AudioData buf {};
        buf.num_channels = 1;
        buf.sample_rate = 44100;
        buf.interleaved_samples.resize(100, 1.0);

        const auto out = TestHelpers::ProcessBufferWithCommand<SeamlessLoopCommand>("seamless-loop 10", buf);
        REQUIRE(out);
        for (auto s : out->interleaved_samples) {
            CHECK(s == doctest::Approx(1.0).epsilon(0.2));
        }
    }
    SUBCASE("zcross") {
        const fs::path in_filename = TEST_DATA_DIRECTORY "/sawtooth_unlooped.flac";
        const fs::path out_filename = "sawtooth_looped.flac";
        auto f = ReadAudioFile(in_filename);

        REQUIRE(f.has_value());

        const auto out =
            TestHelpers::ProcessBufferWithCommand<SeamlessLoopCommand>("seamless-loop 0", f.value());
        REQUIRE(out);

        REQUIRE(WriteAudioFile(out_filename, out.value(), 16));
    }
}
