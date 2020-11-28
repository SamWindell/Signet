#include "common.h"

#include <regex>
#include <system_error>

#include "doctest.hpp"
#include "filesystem.hpp"
#include "rang.hpp"

#include "types.h"

static bool g_messages_enabled = true;

bool GetMessagesEnabled() { return g_messages_enabled; }
void SetMessagesEnabled(bool v) { g_messages_enabled = v; }

void PrintErrorPrefix() {
    std::cout << rang::fg::red << rang::style::bold << "ERROR: " << rang::fg::reset << rang::style::reset;
}

void PrintWarningPrefix() {
    std::cout << rang::fg::yellow << rang::style::bold << "WARNING: " << rang::fg::reset
              << rang::style::reset;
}

void PrintMessagePrefix(const std::string_view heading) {
    std::cout << rang::style::bold << "[" << heading << "]: " << rang::style::reset;
}

void PrintDebugPrefix() { std::cout << rang::style::bold << "DEBUG: " << rang::style::reset; }

void ForEachDeinterleavedChannel(
    const std::vector<double> &interleaved_samples,
    const unsigned num_channels,
    std::function<void(const std::vector<double> &, unsigned channel)> callback) {

    if (num_channels == 1) {
        callback(interleaved_samples, 0);
        return;
    }

    const auto num_frames = interleaved_samples.size() / num_channels;
    std::vector<double> channel_buffer;
    channel_buffer.reserve(num_frames);
    for (unsigned chan = 0; chan < num_channels; ++chan) {
        channel_buffer.clear();
        for (size_t frame = 0; frame < num_frames; ++frame) {
            channel_buffer.push_back(interleaved_samples[frame * num_channels + chan]);
        }

        callback(channel_buffer, chan);
    }
}

double GetCentsDifference(const double pitch1_hz, const double pitch2_hz) {
    constexpr double cents_in_octave = 100 * 12;
    const double cents = std::log2(pitch2_hz / pitch1_hz) * cents_in_octave;
    return cents;
}

double GetFreqWithCentDifference(const double starting_hz, const double cents) {
    constexpr auto cents_in_octave = 100.0 * 12.0;
    const auto multiplier = std::pow(2, cents / cents_in_octave);
    return starting_hz * multiplier;
}

std::unique_ptr<FILE, void (*)(FILE *)> OpenFile(const fs::path &path, const char *mode) {
    static const auto SafeFClose = [](FILE *f) {
        if (f) fclose(f);
    };

    int ec;
#if _WIN32
    FILE *f;
    std::array<WCHAR, 8> wchar_mode {};
    for (size_t i = 0; i < wchar_mode.size() - 1; ++i) {
        wchar_mode[i] = mode[i];
        if (mode[i] == '\0') break;
    }
    ec = _wfopen_s(&f, path.wstring().data(), wchar_mode.data());
    if (ec == 0) {
        return {f, SafeFClose};
    }
#else
    auto f = std::fopen(path.string().data(), mode);
    if (f) {
        return {f, SafeFClose};
    }
    ec = errno;
#endif

    std::error_code std_ec {ec, std::generic_category()};
    WarningWithNewLine("could not open file ", path, " for reason: ", std_ec.message());
    return {nullptr, SafeFClose};
}

TEST_CASE("Common") {
    {
        REQUIRE(GetFreqWithCentDifference(100, 1200) == 200);
        REQUIRE(GetFreqWithCentDifference(100, -1200) == 50);
    }
}
