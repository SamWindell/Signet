#include "reverse.h"

#include "common.h"
#include "test_helpers.h"
#include <algorithm>

CLI::App *ReverseCommand::CreateCommandCLI(CLI::App &app) {
    auto reverse = app.add_subcommand("reverse", "Reverses the audio in the file(s).");
    return reverse;
}

void ReverseCommand::ProcessFiles(AudioFiles &files) {
    for (auto &f : files) {
        auto &audio = f.GetWritableAudio();
        if (audio.IsEmpty()) continue;

        MessageWithNewLine(GetName(), f, "Reversing audio");

        std::reverse(audio.interleaved_samples.begin(), audio.interleaved_samples.end());
        audio.AudioDataWasReversed();
    }
}

TEST_CASE("ReverseCommand") {
    const auto buf = TestHelpers::CreateSquareWaveAtFrequency(1, 44100, 0.2, 440);

    SUBCASE("reverses samples") {
        const auto out = TestHelpers::ProcessBufferWithCommand<ReverseCommand>("reverse", buf);
        REQUIRE(out);

        REQUIRE(out->interleaved_samples.size() == buf.interleaved_samples.size());

        for (size_t i = 0; i < buf.interleaved_samples.size(); i++) {
            REQUIRE(out->interleaved_samples[i] ==
                    buf.interleaved_samples[buf.interleaved_samples.size() - 1 - i]);
        }
    }
}
