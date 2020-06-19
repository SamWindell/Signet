#pragma once

#include "audio_file.h"
#include "subcommand.h"

class Tuner final : public Subcommand {
  public:
    CLI::App *CreateSubcommandCLI(CLI::App &app) override;
    void ProcessFiles(const tcb::span<InputAudioFile> files) override;

    static void ChangePitch(AudioFile &audio, double cents);

  private:
    double m_tune_cents {};
};
