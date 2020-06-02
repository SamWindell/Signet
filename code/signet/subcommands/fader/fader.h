#pragma once

#include "audio_duration.h"
#include "signet_interface.h"
#include "span.hpp"

class Fader final : public Subcommand {
  public:
    enum class Shape { Linear, Sine, SCurve, Log, Exp, Sqrt };

    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    std::optional<AudioFile> Process(const AudioFile &input, ghc::filesystem::path &output_filename) override;
    void Run(SignetInterface &signet) override { signet.ProcessAllFiles(*this); }

  private:
    Shape m_fade_out_shape = Shape::Log;
    Shape m_fade_in_shape = Shape::Log;
    std::optional<AudioDuration> m_fade_out_duration {};
    std::optional<AudioDuration> m_fade_in_duration {};
};
