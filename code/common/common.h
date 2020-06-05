#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "rang.hpp"

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;
using s64 = int64_t;
using b8 = s8;
using usize = size_t;

template <typename Arg, typename... Args>
void ErrorWithNewLine(Arg &&arg, Args &&... args) {
    std::cout << rang::fg::red << rang::style::bold << "ERROR: " << rang::fg::reset << rang::style::reset;
    std::cout << std::forward<Arg>(arg);
    ((std::cout << std::forward<Args>(args)), ...);
    std::cout << "\n";
}

template <typename Arg, typename... Args>
void WarningWithNewLine(Arg &&arg, Args &&... args) {
    std::cout << rang::fg::yellow << rang::style::bold << "WARNING: " << rang::fg::reset
              << rang::style::reset;
    std::cout << std::forward<Arg>(arg);
    ((std::cout << std::forward<Args>(args)), ...);
    std::cout << "\n";
}

template <typename Arg, typename... Args>
void MessageWithNewLine(const std::string_view heading, Arg &&arg, Args &&... args) {
    std::cout << rang::style::bold << "[" << heading << "]: " << rang::style::reset;
    std::cout << std::forward<Arg>(arg);
    ((std::cout << std::forward<Args>(args)), ...);
    std::cout << "\n";
}

template <typename V, typename... T>
constexpr auto MakeArray(T &&... t) -> std::array<V, sizeof...(T)> {
    return {{std::forward<T>(t)...}};
}

static constexpr auto half_pi = 1.57079632679f;
static constexpr auto pi = 3.14159265359f;

inline double DBToAmp(const double d) { return std::pow(10.0, d / 20.0); }
inline double AmpToDB(const double a) { return 20.0 * std::log10(a); }

void ForEachDeinterleavedChannel(const std::vector<double> &interleaved_samples,
                                 const unsigned num_channels,
                                 std::function<void(const std::vector<double> &, unsigned channel)> callback);

inline bool EndsWith(std::string_view str, std::string_view suffix) {
    return suffix.size() <= str.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline bool StartsWith(std::string_view str, std::string_view prefix) {
    return prefix.size() <= str.size() && str.compare(0, prefix.size(), prefix) == 0;
}

inline bool Contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

bool PatternMatch(std::string_view pattern, std::string_view name);

namespace ghc {
namespace filesystem {
class path;
}
} // namespace ghc

std::unique_ptr<FILE, void (*)(FILE *)> OpenFile(const ghc::filesystem::path &path, const char *mode);

std::string WrapText(const std::string &text, const unsigned width, const usize indent_spaces = 0);
