#include "string_utils.h"

#include <regex>

#include "doctest.hpp"
#include "filesystem.hpp"

bool EndsWith(std::string_view str, std::string_view suffix) {
    return suffix.size() <= str.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool StartsWith(std::string_view str, std::string_view prefix) {
    return prefix.size() <= str.size() && str.compare(0, prefix.size(), prefix) == 0;
}

bool Contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

std::string PutNumberInAngleBracket(usize num) { return "<" + std::to_string(num) + ">"; }

bool Replace(std::string &str, std::string_view a, std::string_view b) {
    bool replaced = false;
    usize pos;
    while ((pos = str.find(a.data(), 0, a.size())) != std::string::npos) {
        str.replace(pos, a.size(), b.data(), b.size());
        replaced = true;
    }
    return replaced;
}

bool RegexReplace(std::string &str, std::string pattern, std::string replacement) {
    const std::regex r {pattern};
    const auto result = std::regex_replace(str, r, replacement);
    if (result != str) {
        str = result;
        return true;
    }
    return false;
}

static bool NeedsRegexEscape(const char c) {
    static const std::string_view special_chars = R"([]-{}()*+?.\^$|)";
    for (const char &special_char : special_chars) {
        if (c == special_char) {
            return true;
        }
    }
    return false;
}

bool WildcardMatch(std::string_view pattern, std::string_view name) {
    std::string re_pattern;
    re_pattern.reserve(pattern.size() * 2);

    for (usize i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '*') {
            if (i + 1 < pattern.size() && pattern[i + 1] == '*') {
                re_pattern += ".*";
                i++;
            } else {
                re_pattern += "[^\\/]*";
            }
        } else {
            if (NeedsRegexEscape(pattern[i])) {
                re_pattern += '\\';
            }
            re_pattern += pattern[i];
        }
    }

    const std::regex regex {re_pattern};
    return std::regex_match(std::string(name), regex);
}

std::string GetJustFilenameWithNoExtension(fs::path path) {
    auto filename = path.filename();
    filename.replace_extension("");
    return filename.generic_string();
}

TEST_CASE("String Utils") {
    std::string s {"th<>sef<> < seofi>"};
    REQUIRE(Replace(s, "<>", ".."));
    REQUIRE(s == "th..sef.. < seofi>");

    s = "only one";
    REQUIRE(Replace(s, "one", "two"));
    REQUIRE(s == "only two");

    REQUIRE(!Replace(s, "foo", ""));
}
