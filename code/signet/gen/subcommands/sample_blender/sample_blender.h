#pragma once

#include "CLI11.hpp"
#include "filesystem.hpp"

class SampleBlender {
  public:
    static void Create(CLI::App &app);

  private:
    void Run();

    std::string m_regex;
    fs::path m_directory;
    int m_semitone_interval;
    std::string m_out_filename;
};
