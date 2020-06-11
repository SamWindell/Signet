#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>

#include "span.hpp"

#include "audio_duration.h"
#include "audio_file.h"
#include "common.h"
#include "signet_interface.h"

class ZeroCrossingOffsetter final : public Subcommand {
  public:
    static size_t FindFrameNearestToZeroInBuffer(const tcb::span<const double> interleaved_buffer,
                                                 const size_t num_frames,
                                                 const unsigned num_channels);

    static bool CreateSampleOffsetToNearestZCross(AudioFile &input,
                                                  const AudioDuration &search_size,
                                                  const bool append_skipped_frames_on_end);

    bool ProcessAudio(AudioFile &input, const std::string_view file_name) override {
        if (!input.interleaved_samples.size()) return false;
        return CreateSampleOffsetToNearestZCross(input, m_search_size, m_append_skipped_frames_on_end);
    }

    void Run(SubcommandHost &processor) override { processor.ProcessAllFiles(*this); }

    CLI::App *CreateSubcommandCLI(CLI::App &app) override {
        auto zcross =
            app.add_subcommand("zcross-offset", "Offsets the start of an audio file to the nearest "
                                                "zero-crossing (or the closest thing to a zero crossing)");
        zcross->add_flag(
            "--append", m_append_skipped_frames_on_end,
            "Append the frames offsetted to the end of the file - useful when the sample is a seamless loop");

        zcross
            ->add_option("search_size", m_search_size,
                         WrapText("The maximum length that it is allowed to offset to. " +
                                      AudioDuration::TypeDescription(),
                                  80))
            ->required();
        return zcross;
    }

  private:
    bool m_append_skipped_frames_on_end = false;
    AudioDuration m_search_size {AudioDuration::Unit::Seconds, 1.0};
};
