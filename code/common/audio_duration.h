#pragma once
#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "CLI11.hpp"
#include "doctest.hpp"
#include "magic_enum.hpp"

#include "common.h"
#include "string_utils.h"

class AudioDuration {
  public:
    enum class Unit {
        Seconds,
        Milliseconds,
        Percent,
        Samples,
    };

    AudioDuration(Unit unit, double value) : m_unit(unit), m_value(value) {}

    AudioDuration(const std::string &string) {
        const auto unit = GetUnit(string);
        if (!unit) {
            throw CLI::ValidationError(
                "AudioDuration",
                "This value must be a number of sample or a number followed by one of these units: " +
                    GetListOfUnits());
        }
        m_unit = *unit;
        m_value = std::stof(string);
    }

    bool operator==(const AudioDuration &other) const {
        return other.m_value == m_value && other.m_unit == m_unit;
    }

    size_t GetDurationAsFrames(unsigned sample_rate, size_t num_frames) const {
        double result {};
        switch (m_unit) {
            case Unit::Seconds: result = sample_rate * m_value; break;
            case Unit::Milliseconds: result = sample_rate * (m_value / 1000.0); break;
            case Unit::Percent: result = num_frames * (std::clamp(m_value, 0.0, 100.0) / 100.0); break;
            case Unit::Samples: result = m_value; break;
            default: REQUIRE(0);
        }
        return std::min(num_frames, (size_t)result);
    }

    Unit GetUnit() const { return m_unit; }
    double GetValue() const { return m_value; }

    static std::optional<Unit> GetUnit(const std::string &str) {
        std::string_view suffix {str};
        const auto suffix_size = str.find_first_not_of("0123456789.-");
        if (suffix_size == std::string::npos) {
            // It's only a number, we default to samples
            return Unit::Samples;
        }
        suffix.remove_prefix(suffix_size);

        for (const auto &u : available_units) {
            if (suffix == u.first) {
                return u.second;
            }
        }
        return {};
    }

    static std::string GetListOfUnits() {
        std::string result;
        for (const auto &u : available_units) {
            result.append(u.first);
            result.append(" ");
        }
        return result;
    }

    static std::string ValidatorDescription() { return "AUDIO-DURATION"; }

    static std::string TypeDescription() {
        return "This value is a number in samples, or a number directly followed by a unit: the unit can be one of {" +
               GetCommaSeparatedUnits() + "}. These represent {" + GetCommaSeparatedUnitsNames() +
               "} respectively. The percent option specifies the duration relative to the whole "
               "length of the sample. Examples of audio durations are: 5s, 12.5%, 250ms or 42909smp.";
    }

  private:
    static std::string GetCommaSeparatedUnits() {
        std::string result;
        static const std::string divider = ", ";
        for (auto [unit, value] : available_units) {
            result += std::string(unit) + divider;
        }
        return result.substr(0, result.size() - divider.size());
    }

    static std::string GetCommaSeparatedUnitsNames() {
        std::string result;
        static const std::string divider = ", ";
        for (const auto &name : magic_enum::enum_names<Unit>()) {
            result += std::string(name) + divider;
        }
        return result.substr(0, result.size() - divider.size());
    }

    static constexpr std::pair<const char *, Unit> available_units[] = {
        {"s", Unit::Seconds},
        {"ms", Unit::Milliseconds},
        {"%", Unit::Percent},
        {"smp", Unit::Samples},
    };
    Unit m_unit {};
    double m_value {};
};
