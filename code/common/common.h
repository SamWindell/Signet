#pragma once
#include <array>
#include <cmath>
#include <iostream>
#include <string_view>

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

inline float DBToAmp(const float d) { return std::pow(10.0f, d / 20.0f); }

inline bool EndsWith(std::string_view str, std::string_view suffix) {
    return suffix.size() <= str.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline bool StartsWith(std::string_view str, std::string_view prefix) {
    return prefix.size() <= str.size() && str.compare(0, prefix.size(), prefix) == 0;
}
