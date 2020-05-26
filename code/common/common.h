#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string_view>

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
void FatalErrorWithNewLine(Arg &&arg, Args &&... args) {
    std::cout << "ERROR: ";
    std::cout << std::forward<Arg>(arg);
    ((std::cout << std::forward<Args>(args)), ...);
    std::cout << "\n";
    exit(1);
}

template <typename Arg, typename... Args>
void WarningWithNewLine(Arg &&arg, Args &&... args) {
    std::cout << "WARNING: ";
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

inline float DBToAmp(const float d) { return std::pow(10.0f, d / 20.0f); }
inline float AmpToDB(const float a) { return 20.0f * std::log10(a); }

inline bool EndsWith(std::string_view str, std::string_view suffix) {
    return suffix.size() <= str.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline bool StartsWith(std::string_view str, std::string_view prefix) {
    return prefix.size() <= str.size() && str.compare(0, prefix.size(), prefix) == 0;
}
