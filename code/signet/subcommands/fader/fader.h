#pragma once

#include "audio_duration.h"
#include "signet_interface.h"
#include "span.hpp"

class Fader final : public Subcommand {
  public:
    enum class Shape { Linear, Sine, SCurve, Log, Exp, Sqrt };

    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    bool Process(AudioFile &input) override;
    void Run(SignetInterface &signet) override { signet.ProcessAllFiles(*this); }

  private:
    Shape m_fade_out_shape = Shape::Sine;
    Shape m_fade_in_shape = Shape::Sine;
    std::optional<AudioDuration> m_fade_out_duration {};
    std::optional<AudioDuration> m_fade_in_duration {};
};
