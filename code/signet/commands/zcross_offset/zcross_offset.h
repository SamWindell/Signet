#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>

#include "span.hpp"

#include "audio_duration.h"
#include "audio_file_io.h"
#include "common.h"
#include "signet_interface.h"

class ZeroCrossOffsetCommand final : public Command {
  public:
    static size_t FindFrameNearestToZeroInBuffer(const tcb::span<const double> interleaved_buffer,
                                                 const size_t num_frames,
                                                 const unsigned num_channels);

    static bool CreateSampleOffsetToNearestZCross(AudioData &audio,
                                                  const AudioDuration &search_size,
                                                  const bool append_skipped_frames_on_end);

    void ProcessFiles(AudioFiles &files) override {
        for (auto &f : files) {
            auto &audio = f.GetAudio();
            if (audio.IsEmpty()) continue;
            CreateSampleOffsetToNearestZCross(f.GetWritableAudio(), m_search_size,
                                              m_append_skipped_frames_on_end);
        }
    }
    std::string GetName() const override { return GetNameInternal(); }
    static std::string GetNameInternal() { return "ZeroCrossOffset"; }

    CLI::App *CreateCommandCLI(CLI::App &app) override {
        auto zcross = app.add_subcommand(
            "zcross-offset", "Offsets the start of an audio file to the nearest "
                             "zero-crossing (or the closest thing to a zero crossing). You can use the "
                             "option --append to cause the samples that were offsetted to be appended to the "
                             "end of the file. This is useful for when the file is a seamless loop.");
        zcross->add_flag("--append", m_append_skipped_frames_on_end,
                         "Append the frames offsetted to the end of the file - useful when the sample is a "
                         "seamless loop.");

        zcross
            ->add_option("search_size", m_search_size,
                         "The maximum length that it is allowed to offset to. " +
                             AudioDuration::TypeDescription())
            ->required();
        return zcross;
    }

  private:
    bool m_append_skipped_frames_on_end = false;
    AudioDuration m_search_size {AudioDuration::Unit::Seconds, 1.0};
};
