#include "common.h"

#include <regex>
#include <system_error>

#include "doctest.hpp"
#include "filesystem.hpp"
#include "fmt/color.h"

#include "types.h"

bool g_messages_enabled = true;
bool g_warning_issued = false;
bool g_error_issued = false;


void PrintErrorPrefix(std::string_view heading) {
    fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "ERROR ({}): ", heading);
}

void PrintWarningPrefix(std::string_view heading) {
    fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "WARNING ({}): ", heading);
}

void PrintMessagePrefix(const std::string_view heading) {
    fmt::print(fmt::emphasis::bold, "[{}]: ", heading);
}

void PrintDebugPrefix() { fmt::print(fmt::emphasis::bold, "[DEBUG]: "); }

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
    if (pitch1_hz == 0) {
        return 0;
    }
    constexpr double cents_in_octave = 100 * 12;
    const double cents = std::log2(pitch2_hz / pitch1_hz) * cents_in_octave;
    return cents;
}

double GetFreqWithCentDifference(const double starting_hz, const double cents) {
    constexpr auto cents_in_octave = 100.0 * 12.0;
    const auto multiplier = std::pow(2, cents / cents_in_octave);
    return starting_hz * multiplier;
}

FILE *OpenFileRaw(const fs::path &path, const char *mode, std::error_code *ec_out) {
    if (ec_out) *ec_out = {};

    int ec;
#if _WIN32
    FILE *f;
    std::array<wchar_t, 8> wchar_mode {};
    for (size_t i = 0; i < wchar_mode.size() - 1; ++i) {
        wchar_mode[i] = mode[i];
        if (mode[i] == '\0') break;
    }
    ec = _wfopen_s(&f, path.wstring().data(), wchar_mode.data());
    if (ec == 0) {
        return f;
    }
#else
    auto f = std::fopen(path.string().data(), mode);
    if (f) {
        return f;
    }
    ec = errno;
#endif

    if (ec_out) *ec_out = {ec, std::generic_category()};
    return nullptr;
}

std::unique_ptr<FILE, void (*)(FILE *)> OpenFile(const fs::path &path, const char *mode) {
    static const auto SafeFClose = [](FILE *f) {
        if (f) {
            const auto result = fclose(f);
            assert(result == 0);
        }
    };

    std::error_code ec {};
    auto f = OpenFileRaw(path, mode, &ec);
    if (f) return {f, SafeFClose};

    WarningWithNewLine("Signet", "could not open file {} for reason: {}", path, ec.message());
    return {nullptr, SafeFClose};
}

TEST_CASE("Common") {
    {
        REQUIRE(GetFreqWithCentDifference(100, 1200) == 200);
        REQUIRE(GetFreqWithCentDifference(100, -1200) == 50);
    }
}
