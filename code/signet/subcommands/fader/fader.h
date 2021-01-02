#pragma once

#include "audio_duration.h"
#include "span.hpp"
#include "subcommand.h"

class Fader final : public Subcommand {
  public:
    enum class Shape { Linear, Sine, SCurve, Log, Exp, Sqrt };

    std::string GetName() const override { return "Fader"; }
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(const tcb::span<EditTrackedAudioFile> files) override;

  private:
    Shape m_fade_out_shape = Shape::Sine;
    Shape m_fade_in_shape = Shape::Sine;
    std::optional<AudioDuration> m_fade_out_duration {};
    std::optional<AudioDuration> m_fade_in_duration {};
};
