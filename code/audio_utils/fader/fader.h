#pragma once

#include "audio_duration.h"
#include "audio_util_interface.h"
#include "span.hpp"

class Fader final : public Processor {
  public:
    enum class Shape { Linear, Sine, SCurve, Log, Exp, Sqrt };

    void AddCLI(CLI::App &app) override;
    std::optional<AudioFile> Process(const AudioFile &input, ghc::filesystem::path &output_filename) override;
    void Run(AudioUtilInterface &audio_util) override { audio_util.ProcessAllFiles(); }
    std::string GetDescription() override { return "Add a fade to the start or end of the samples"; }

    static tcb::span<const std::string_view> GetShapeNames();

  private:
    Shape m_shape = Shape::Linear;
    std::optional<AudioDuration> m_fade_out {};
    std::optional<AudioDuration> m_fade_in {};
};
