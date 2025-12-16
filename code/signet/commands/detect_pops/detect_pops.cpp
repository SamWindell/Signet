#include "detect_pops.h"

#include "CLI11.hpp"
#include "doctest.hpp"
#include "test_helpers.h"

#include <cmath>
#include <numeric>
#include <set>
#include <vector>

// Cubic interpolation function for smooth repairs
// t is a value from 0 to 1 representing the proportion between f0 and f1
inline double InterpolateCubic(double f0, double f1, double f2, double fm1, double t) {
    return (f0 + (((f2 - fm1 - 3 * f1 + 3 * f0) * t + 3 * (f1 + fm1 - 2 * f0)) * t -
                  (f2 + 2 * fm1 - 6 * f1 + 3 * f0)) *
                     t / 6.0);
}

CLI::App *DetectPopsCommand::CreateCommandCLI(CLI::App &app) {
    auto detect_pops = app.add_subcommand(
        "detect-pops",
        "Detects pops (sudden discontinuities) in audio files and prints the frame locations where they occur. "
        "Uses second derivative analysis to detect samples that suddenly break from the recent waveform curvature.");

    detect_pops->add_option(
        "--threshold", m_threshold,
        "Sensitivity threshold. A pop is detected when the second derivative (acceleration) of the signal "
        "deviates by this many standard deviations from the global average. Higher values = less sensitive. "
        "Typical range: 10.0 to 50.0. Default is 30.0.")
        ->check(CLI::Range(0.5, 100.0));

    detect_pops->add_flag(
        "--fix", m_fix,
        "Automatically repair detected pops using cubic interpolation. "
        "Replaces glitched samples with smoothly interpolated values based on surrounding clean audio.");

    detect_pops->add_flag(
        "--zero-only", m_zero_only,
        "Only detect pops where the outlier sample value is zero (or within epsilon of zero). "
        "Useful for finding glitches where samples are randomly set to 0, without false positives from high-frequency content.");

    return detect_pops;
}

std::vector<PopLocation> DetectPopsCommand::DetectPops(const AudioData &audio) const {
    const auto num_frames = audio.NumFrames();
    const auto num_channels = audio.num_channels;

    std::vector<PopLocation> detected_pops;

    // Check each channel independently
    for (unsigned channel = 0; channel < num_channels; ++channel) {
        // Calculate second derivatives for all samples in this channel
        std::vector<double> second_derivatives;
        second_derivatives.reserve(num_frames);

        // Need at least 2 samples to calculate second derivative
        for (size_t frame = 2; frame < num_frames; ++frame) {
            const double prev_prev = audio.GetSample(channel, frame - 2);
            const double prev = audio.GetSample(channel, frame - 1);
            const double curr = audio.GetSample(channel, frame);

            // Second derivative: curr - 2*prev + prev_prev
            const double second_deriv = curr - 2.0 * prev + prev_prev;
            second_derivatives.push_back(second_deriv);
        }

        if (second_derivatives.size() < 3) continue;

        // Calculate global statistics
        const double mean = std::accumulate(second_derivatives.begin(), second_derivatives.end(), 0.0) /
                            second_derivatives.size();

        double variance_sum = 0.0;
        for (const auto &val : second_derivatives) {
            const double diff = val - mean;
            variance_sum += diff * diff;
        }
        const double stddev = std::sqrt(variance_sum / (second_derivatives.size() - 1));

        // Add epsilon to avoid false positives in silent sections
        const double threshold_value = m_threshold * std::max(stddev, 0.00001);

        // Find outliers
        std::vector<std::pair<size_t, double>> outliers; // frame, deviation
        for (size_t i = 0; i < second_derivatives.size(); ++i) {
            const double current_deriv = second_derivatives[i];
            const double deviation = std::abs(current_deriv - mean);

            if (deviation > threshold_value) {
                const size_t frame = i + 2; // Offset because second_derivatives starts at frame 2

                // If zero-only mode is enabled, check if the outlier sample is near zero
                if (m_zero_only) {
                    const size_t outlier_frame = frame > 0 ? frame - 1 : frame;
                    const double outlier_value = audio.GetSample(channel, outlier_frame);
                    const double epsilon = 0.0001; // Small epsilon for floating point comparison

                    if (std::abs(outlier_value) > epsilon) {
                        continue; // Skip this outlier - it's not a zero-sample glitch
                    }
                }

                outliers.push_back({frame, deviation});
            }
        }

        // Group adjacent outliers and keep only the most extreme one in each cluster
        if (!outliers.empty()) {
            size_t cluster_start = 0;
            for (size_t i = 1; i <= outliers.size(); ++i) {
                bool is_end = (i == outliers.size());
                bool is_gap = !is_end && (outliers[i].first - outliers[i - 1].first > 1);

                if (is_end || is_gap) {
                    // Find the frame with maximum deviation in this cluster
                    size_t max_idx = cluster_start;
                    for (size_t j = cluster_start + 1; j < i; ++j) {
                        if (outliers[j].second > outliers[max_idx].second) {
                            max_idx = j;
                        }
                    }

                    detected_pops.push_back({outliers[max_idx].first, channel, outliers[max_idx].second});
                    cluster_start = i;
                }
            }
        }
    }

    return detected_pops;
}

void DetectPopsCommand::RepairPops(AudioData &audio, const std::vector<PopLocation> &pops) const {
    const auto num_frames = audio.NumFrames();
    const auto num_channels = audio.num_channels;
    int skipped_count = 0;

    // Collect unique frames where pops were detected on any channel
    std::set<size_t> frames_to_repair;
    for (const auto &pop : pops) {
        // The detected frame is where the second derivative spikes (when returning to normal)
        // The actual outlier sample is one frame earlier
        const size_t repair_frame = pop.frame > 0 ? pop.frame - 1 : pop.frame;

        // Check boundaries: need 2 samples before and after for cubic interpolation
        if (repair_frame < 2 || repair_frame >= num_frames - 2) {
            skipped_count++;
            continue;
        }

        frames_to_repair.insert(repair_frame);
    }

    // For each unique frame, repair ALL channels using independent interpolation
    for (size_t repair_frame : frames_to_repair) {
        for (unsigned channel = 0; channel < num_channels; ++channel) {
            // Gather 4 surrounding samples (skip the pop itself at repair_frame)
            const double fm1 = audio.GetSample(channel, repair_frame - 2);
            const double f0 = audio.GetSample(channel, repair_frame - 1);
            const double f1 = audio.GetSample(channel, repair_frame + 1);
            const double f2 = audio.GetSample(channel, repair_frame + 2);

            // Interpolate at the midpoint (t = 0.5) between f0 and f1
            const double interpolated = InterpolateCubic(f0, f1, f2, fm1, 0.5);

            // Replace the pop sample with the interpolated value
            audio.GetSample(channel, repair_frame) = interpolated;
        }
    }

    if (skipped_count > 0) {
        WarningWithNewLine(GetName(), {}, "Skipped {} pop(s) too close to file boundaries", skipped_count);
    }
}

void DetectPopsCommand::ReportDetections(EditTrackedAudioFile &f, const std::vector<PopLocation> &pops) const {
    if (pops.empty()) {
        MessageWithNewLine(GetName(), f, "none-detected");
    } else {
        // Sort by frame (stable sort to maintain channel order for same frame)
        auto sorted_pops = pops;
        std::stable_sort(sorted_pops.begin(), sorted_pops.end(),
                         [](const PopLocation &a, const PopLocation &b) { return a.frame < b.frame; });

        std::string pop_locations;
        for (size_t i = 0; i < sorted_pops.size(); ++i) {
            if (i > 0) pop_locations += ", ";
            pop_locations += std::to_string(sorted_pops[i].frame);

            // For multi-channel files, show channel indicator
            if (f.GetAudio().num_channels > 1) {
                const char *channel_name = sorted_pops[i].channel == 0 ? "L" : "R";
                pop_locations += " (" + std::string(channel_name) + ")";
            }
        }

        MessageWithNewLine(GetName(), f, "Pops detected at frame{}: {}",
                           pops.size() > 1 ? "s" : "", pop_locations);
    }
}

void DetectPopsCommand::ReportRepairs(EditTrackedAudioFile &f, const std::vector<PopLocation> &pops) const {
    if (pops.empty()) {
        MessageWithNewLine(GetName(), f, "none-detected");
    } else {
        // Sort by frame (stable sort to maintain channel order for same frame)
        auto sorted_pops = pops;
        std::stable_sort(sorted_pops.begin(), sorted_pops.end(),
                         [](const PopLocation &a, const PopLocation &b) { return a.frame < b.frame; });

        std::string pop_locations;
        for (size_t i = 0; i < sorted_pops.size(); ++i) {
            if (i > 0) pop_locations += ", ";
            pop_locations += std::to_string(sorted_pops[i].frame);

            // For multi-channel files, show channel indicator
            if (f.GetAudio().num_channels > 1) {
                const char *channel_name = sorted_pops[i].channel == 0 ? "L" : "R";
                pop_locations += " (" + std::string(channel_name) + ")";
            }
        }

        MessageWithNewLine(GetName(), f, "Detected and repaired {} pop{} at frame{}: {}",
                           pops.size(), pops.size() > 1 ? "s" : "", pops.size() > 1 ? "s" : "", pop_locations);
    }
}

void DetectPopsCommand::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        // Detect pops (read-only operation)
        auto detected_pops = DetectPops(f.GetAudio());

        if (m_fix) {
            // Repair mode: only get writable audio if there are pops to fix
            if (!detected_pops.empty()) {
                RepairPops(f.GetWritableAudio(), detected_pops);
            }
            ReportRepairs(f, detected_pops);
        } else {
            // Detection-only mode: just report
            ReportDetections(f, detected_pops);
        }
    }
}

TEST_CASE("DetectPopsCommand") {
    SUBCASE("No pops in clean sine wave") {
        auto test_audio = TestHelpers::CreateSingleOscillationSineWave(2, 44100, 44100);
        TestHelpers::ProcessBufferWithCommand<DetectPopsCommand>("detect-pops", test_audio);
        // Should report none-detected - a smooth sine wave has no discontinuities
    }

    SUBCASE("Detect single-sample spike (classic pop)") {
        auto test_audio = TestHelpers::CreateSingleOscillationSineWave(1, 44100, 44100);

        // Create an artificial discontinuity by injecting a sudden jump
        // This simulates a classic "pop" - a single sample that spikes
        const size_t glitch_frame = 1000;
        const double original = test_audio.GetSample(0, glitch_frame);
        test_audio.GetSample(0, glitch_frame) = original + 0.5; // Sharp jump up
        test_audio.GetSample(0, glitch_frame + 1) = original; // Back to normal

        TestHelpers::ProcessBufferWithCommand<DetectPopsCommand>("detect-pops", test_audio);
        // Should detect a pop at frame 1001 (the second derivative spike occurs when returning to normal)
    }

    SUBCASE("Detect DC offset jump (sudden level shift)") {
        auto test_audio = TestHelpers::CreateSingleOscillationSineWave(1, 44100, 44100);

        // Simulate a DC offset jump - this is like the glitch shown in the user's image
        // The waveform suddenly jumps to a different DC level
        const size_t jump_frame = 5000;
        for (size_t i = jump_frame; i < test_audio.NumFrames(); ++i) {
            test_audio.GetSample(0, i) += 0.2;
        }

        TestHelpers::ProcessBufferWithCommand<DetectPopsCommand>("detect-pops", test_audio);
        // Should detect a discontinuity at the frame where the DC offset changes
    }

    SUBCASE("Ignore normal high-frequency content") {
        // High-frequency sine waves have large second derivatives but shouldn't be flagged as pops
        auto test_audio = TestHelpers::CreateSineWaveAtFrequency(1, 44100, 1.0, 8000.0);

        TestHelpers::ProcessBufferWithCommand<DetectPopsCommand>("detect-pops", test_audio);
        // Should report none-detected - high frequency is not a discontinuity
    }

    SUBCASE("Adjust sensitivity with threshold parameter") {
        auto test_audio = TestHelpers::CreateSingleOscillationSineWave(1, 44100, 44100);

        // Create a moderate discontinuity
        const size_t glitch_frame = 500;
        const double original = test_audio.GetSample(0, glitch_frame);
        test_audio.GetSample(0, glitch_frame) = original + 0.2;

        // Lower threshold (more sensitive) should detect it
        TestHelpers::ProcessBufferWithCommand<DetectPopsCommand>("detect-pops --threshold 2.0", test_audio);

        // Higher threshold (less sensitive) might not detect it
        TestHelpers::ProcessBufferWithCommand<DetectPopsCommand>("detect-pops --threshold 10.0", test_audio);
    }
}
