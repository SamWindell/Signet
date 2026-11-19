#include "common.h"

#include <cstdlib>
#include <regex>
#include <system_error>

#if _WIN32
#include <windows.h>
#endif

#include "doctest.hpp"
#include "filesystem.hpp"
#include "fmt/color.h"

#include "edit_tracked_audio_file.h"
#include "types.h"

bool g_messages_enabled = true;
bool g_warnings_as_errors = false;

bool g_color_enabled = true;

#if _WIN32
// Returns false only if the handle is a console that cannot be put into virtual terminal mode. Handles that
// are not consoles (e.g. redirected to a file or pipe) return true since there is nothing to enable.
static bool EnableVTMode(DWORD std_handle) {
    HANDLE handle = GetStdHandle(std_handle);
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr) return true;

    DWORD mode = 0;
    if (!GetConsoleMode(handle, &mode)) return true;
    if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) return true;
    return SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}
#endif

void SetupConsoleColor(const bool no_color_flag) {
    const char *no_color_env = std::getenv("NO_COLOR");
    if (no_color_flag || (no_color_env && no_color_env[0] != '\0')) {
        g_color_enabled = false;
        return;
    }

#if _WIN32
    const bool stdout_ok = EnableVTMode(STD_OUTPUT_HANDLE);
    const bool stderr_ok = EnableVTMode(STD_ERROR_HANDLE);
    g_color_enabled = stdout_ok && stderr_ok;
#else
    g_color_enabled = true;
#endif
}

void PrintFilename(FILE *stream, const EditTrackedAudioFile &f) {
    fmt::print(stream, ": ");
    fmt::print(stream, Style(fg(fmt::color::navajo_white)), "{}", f.OriginalFilename());
}
void PrintFilename(FILE *stream, const fs::path &path) {
    fmt::print(stream, ": ");
    fmt::print(stream, Style(fg(fmt::color::navajo_white)), "{}", GetJustFilenameWithNoExtension(path));
}
void PrintFilename(FILE *, NoneType) {}

void PrintErrorPrefix(FILE *stream, std::string_view heading) {
    fmt::print(stream, Style(fmt::fg(fmt::color::red) | fmt::emphasis::bold), "[{}] ERROR", heading);
    fmt::print(stream, ": ");
}
void PrintWarningPrefix(FILE *stream, std::string_view heading) {
    fmt::print(stream, Style(fmt::fg(fmt::color::orange) | fmt::emphasis::bold), "[{}] WARNING", heading);
    fmt::print(stream, ": ");
}
void PrintMessagePrefix(FILE *stream, const std::string_view heading) {
    fmt::print(stream, Style(fmt::fg(fmt::color::cornflower_blue) | fmt::emphasis::bold), "[{}]", heading);
    fmt::print(stream, ": ");
}

void PrintDebugPrefix(FILE *stream) { fmt::print(stream, Style(fmt::emphasis::bold), "[DEBUG]: "); }

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

#if _WIN32
std::wstring ConvertUTF8ToUTF16(const char *utf8) {
    const auto utf8_size = std::strlen(utf8);
    const auto utf16_size = MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_size, nullptr, 0);
    std::wstring utf16(utf16_size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, utf8_size, utf16.data(), utf16_size);
    return utf16;
}
#endif

FILE *OpenFileRaw(const fs::path &path, const char *mode, std::error_code *ec_out) {
    if (ec_out) *ec_out = {};

    int ec;
#if _WIN32
    FILE *f;
    const auto wchar_mode = ConvertUTF8ToUTF16(mode);
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

    WarningWithNewLine("Signet", {}, "could not open file {} for reason: {}", path, ec.message());
    return {nullptr, SafeFClose};
}

std::string ReadEntireFile(const fs::path &path) {
    std::string result {};
    auto f = OpenFile(path, "rb");
    if (!f) return result;

    fseek(f.get(), 0, SEEK_END);
    const auto size = ftell(f.get());
    fseek(f.get(), 0, SEEK_SET);

    result.resize(size);
    auto const num_read = fread(result.data(), 1, size, f.get());
    if (num_read != (size_t)size) {
        WarningWithNewLine("Signet", {}, "could not read file {} for reason: {}", path, strerror(errno));
        return {};
    }
    return result;
}

TEST_CASE("Common") {
    {
        REQUIRE(GetFreqWithCentDifference(100, 1200) == 200);
        REQUIRE(GetFreqWithCentDifference(100, -1200) == 50);
    }
}
