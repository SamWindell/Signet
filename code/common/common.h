#pragma once
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <fmt/core.h>

#include "filesystem.hpp"

extern bool g_messages_enabled;
extern bool g_warnings_as_errors;

struct EditTrackedAudioFile;

template <>
struct fmt::formatter<fs::path> {
    constexpr auto parse(format_parse_context &ctx) { return ctx.end(); }

    template <typename FormatContext>
    auto format(const fs::path &p, FormatContext &ctx) {
        return format_to(ctx.out(), "\"{}\"", p.generic_string());
    }
};

void PrintErrorPrefix(FILE *f, std::string_view heading);
void PrintWarningPrefix(FILE *f, std::string_view heading);
void PrintMessagePrefix(FILE *f, std::string_view heading);
void PrintDebugPrefix(FILE *f);

struct NoneType {};

struct SignetError : public std::runtime_error {
    SignetError(const std::string &str) : std::runtime_error(str) {}
};
struct SignetWarning : public std::runtime_error {
    SignetWarning(const std::string &str) : std::runtime_error(str) {}
};

void PrintFilename(FILE *stream, const EditTrackedAudioFile &f);
void PrintFilename(FILE *stream, fs::path &path);
void PrintFilename(FILE *stream, NoneType n);

template <typename NameType = NoneType, typename... Args>
void ErrorWithNewLine(std::string_view heading,
                      const NameType &f,
                      std::string_view format,
                      const Args &...args) {
    PrintErrorPrefix(stdout, heading);
    fmt::vprint(format, fmt::make_format_args(args...));
    PrintFilename(stdout, f);
    fmt::print("\n");
    throw SignetError("A fatal error occurred");
}

template <typename NameType = NoneType, typename... Args>
void WarningWithNewLine(std::string_view heading,
                        const NameType &f,
                        std::string_view format,
                        Args &&...args) {
    PrintWarningPrefix(stdout, heading);
    fmt::vprint(format, fmt::make_format_args(args...));
    PrintFilename(stdout, f);
    fmt::print("\n");
    if (g_warnings_as_errors)
        throw SignetWarning("A warning occurred, and warnings are set to be treated as errors");
}

template <typename NameType = NoneType, typename... Args>
void MessageWithNewLine(std::string_view heading,
                        const NameType &f,
                        std::string_view format,
                        Args &&...args) {
    if (g_messages_enabled) {
        PrintMessagePrefix(stdout, heading);
        fmt::vprint(format, fmt::make_format_args(args...));
        PrintFilename(stdout, f);
        fmt::print("\n");
    }
}

template <typename NameType = NoneType, typename... Args>
void StderrMessageWithNewLine(std::string_view heading,
                              const NameType &f,
                              std::string_view format,
                              Args &&...args) {
    if (g_messages_enabled) {
        PrintMessagePrefix(stderr, heading);
        fmt::vprint(stderr, format, fmt::make_format_args(args...));
        PrintFilename(stderr, f);
        fmt::print(stderr, "\n");
    }
}

template <typename... Args>
void DebugWithNewLine([[maybe_unused]] std::string_view format, [[maybe_unused]] Args &&...args) {
#if SIGNET_DEBUG
    PrintDebugPrefix(stdout);
    fmt::vprint(format, fmt::make_format_args(args...));
    fmt::print("\n");
#endif
}

template <typename V, typename... T>
constexpr auto MakeArray(T &&...t) -> std::array<V, sizeof...(T)> {
    return {{std::forward<T>(t)...}};
}

static constexpr auto half_pi = 1.57079632679;
static constexpr auto pi = 3.14159265358979323846;
static constexpr auto sqrt_two = 1.41421356237309504880;

inline double DBToAmp(const double d) { return std::pow(10.0, d / 20.0); }
inline double AmpToDB(const double a) { return 20.0 * std::log10(a); }

void ForEachDeinterleavedChannel(const std::vector<double> &interleaved_samples,
                                 const unsigned num_channels,
                                 std::function<void(const std::vector<double> &, unsigned channel)> callback);

double GetCentsDifference(double pitch1_hz, double pitch2_hz);
double GetFreqWithCentDifference(double starting_hz, double cents);

std::unique_ptr<FILE, void (*)(FILE *)> OpenFile(const fs::path &path, const char *mode);
FILE *OpenFileRaw(const fs::path &path, const char *mode, std::error_code *ec = nullptr);
std::string ReadEntireFile(const fs::path &path);
