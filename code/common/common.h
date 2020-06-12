#pragma once
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "filesystem.hpp"
#include "rang.hpp"

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
    // std::cout << rang::style::bold << "[" << heading << "]: " << rang::style::reset;
    // std::cout << std::forward<Arg>(arg);
    // ((std::cout << std::forward<Args>(args)), ...);
    // std::cout << "\n";
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

double GetCentsDifference(double pitch1_hz, double pitch2_hz);

std::unique_ptr<FILE, void (*)(FILE *)> OpenFile(const fs::path &path, const char *mode);
