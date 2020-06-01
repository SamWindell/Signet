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

    static AudioFile CreateSampleOffsetToNearestZCross(const AudioFile &input,
                                                       const AudioDuration &search_size,
                                                       const bool append_skipped_frames_on_end);

    std::optional<AudioFile> Process(const AudioFile &input,
                                     ghc::filesystem::path &output_filepath) override {
        if (!input.interleaved_samples.size()) return {};
        return CreateSampleOffsetToNearestZCross(input, m_search_size, m_append_skipped_frames_on_end);
    }

    void Run(SignetInterface &util) override { util.ProcessAllFiles(*this); }

    CLI::App *CreateSubcommandCLI(CLI::App &app) override {
        auto zcross = app.add_subcommand(
            "zcross-offset", "Offset the start of a FLAC or WAV file to the nearest zero-crossing");
        zcross->add_flag(
            "-a,--append-skipped", m_append_skipped_frames_on_end,
            "Append the frames offsetted to the end of the file - useful when the sample is a seamless loop");

        zcross
            ->add_option("search_size", m_search_size,
                         "The duration from the start of the sample to search for the zero crossing in")
            ->required()
            ->check(AudioDuration::ValidateString, AudioDuration::ValidatorDescription());
        return zcross;
    }

  private:
    bool m_append_skipped_frames_on_end = false;
    AudioDuration m_search_size {AudioDuration::Unit::Seconds, 1.0};
};
