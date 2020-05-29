#pragma once
#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "doctest.hpp"

#include "common.h"

class AudioDuration {
  public:
    enum class Unit {
        Seconds,
        Milliseconds,
        Percent,
        Samples,
    };

    AudioDuration(Unit unit, float value) : m_unit(unit), m_value(value) {}

    AudioDuration(const std::string &string) {
        m_unit = *GetUnit(string);
        m_value = std::stof(string);
    }

    bool operator==(const AudioDuration &other) const {
        return other.m_value == m_value && other.m_unit == m_unit;
    }

    size_t GetDurationAsFrames(unsigned sample_rate, size_t num_frames) const {
        float result {};
        switch (m_unit) {
            case Unit::Seconds: result = sample_rate * m_value; break;
            case Unit::Milliseconds: result = sample_rate * (m_value / 1000.0f); break;
            case Unit::Percent: result = num_frames * (std::clamp(m_value, 0.0f, 100.0f) / 100.0f); break;
            case Unit::Samples: result = m_value; break;
            default: REQUIRE(0);
        }
        return std::min(num_frames, (size_t)result);
    }

    static std::optional<Unit> GetUnit(const std::string &str) {
        for (const auto u : available_units) {
            if (EndsWith(str, u.first)) {
                return u.second;
            }
        }
        return {};
    }

    static std::string ValidateString(const std::string &str) {
        if (const auto unit = GetUnit(str); unit) {
            return {};
        } else {
            std::string error {"Value must be specified as one of the following units: "};
            for (const auto u : available_units) {
                error.append(u.first);
                error.append(" ");
            }
            return error;
        }
    }

    static std::string ValidatorDescription() { return "Duration unit validator"; }

  private:
    static constexpr std::pair<const char *, Unit> available_units[] = {{"s", Unit::Seconds},
                                                                        {"ms", Unit::Milliseconds},
                                                                        {"%", Unit::Percent},
                                                                        {"smp", Unit::Samples}};
    Unit m_unit {};
    float m_value {};
};
