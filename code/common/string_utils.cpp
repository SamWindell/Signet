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

bool Replace(std::string &str, const char a, const char b) {
    bool changed = false;
    for (auto &c : str) {
        if (c == a) {
            c = b;
            changed = true;
        }
    }
    return changed;
}

bool Replace(std::string &str, std::string_view a, std::string_view b) {
    bool replaced = false;
    usize pos = 0;
    while ((pos = str.find(a.data(), pos, a.size())) != std::string::npos) {
        str.replace(pos, a.size(), b.data(), b.size());
        pos += b.size();
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

bool WildcardMatch(std::string_view pattern, std::string_view str, bool case_insensitive) {
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

    const std::regex regex {re_pattern, case_insensitive ? std::regex::ECMAScript | std::regex::icase
                                                         : std::regex::ECMAScript};

    return std::regex_match(std::string(str), regex);
}

std::string GetJustFilenameWithNoExtension(fs::path path) {
    auto filename = path.filename();
    filename.replace_extension("");
    return filename.generic_string();
}

std::string TrimWhitespace(std::string_view str) {
    while (str.size() && std::isspace(str.back())) {
        str.remove_suffix(1);
    }
    return std::string(str);
}

std::string TrimWhitespaceFront(std::string_view str) {
    while (str.size() && std::isspace(str.front())) {
        str.remove_prefix(1);
    }
    return std::string(str);
}

void Lowercase(std::string &str) {
    for (auto &c : str) {
        c = (char)std::tolower(c);
    }
}

std::string ToSnakeCase(const std::string_view str) {
    std::string result {str};
    Lowercase(result);
    Replace(result, ' ', '_');
    Replace(result, '-', '_');
    return result;
}

std::string ToCamelCase(const std::string_view str) {
    std::string result;
    result.reserve(str.size());
    bool capitalise_next = true;
    for (const auto c : str) {
        if (c == ' ') {
            capitalise_next = true;
        } else {
            result += capitalise_next ? (char)std::toupper(c) : c;
            capitalise_next = std::isdigit(c);
        }
    }
    return result;
}

std::string WrapText(const std::string_view text, const unsigned width) {
    std::string result;
    result.reserve(text.size() * 3 / 2);
    usize col = 0;
    for (usize i = 0; i < text.size(); ++i) {
        const auto c = text[i];
        if (c == '\n' || c == '\r') {
            if (i != (text.size() - 1) &&
                ((c == '\n' && text[i + 1] == '\r') || (c == '\r' && text[i + 1] == '\n'))) {
                i++;
            }
            result += '\n';
            col = 0;
        } else if (col >= width) {
            bool found_space = false;
            int pos = (int)result.size() - 1;
            while (pos >= 0) {
                if (std::isspace(result[pos])) {
                    result[pos] = '\n';
                    found_space = true;
                    break;
                }
                pos--;
            }
            if (found_space) {
                col = (int)result.size() - pos;
            } else {
                result += '\n';
                col = 0;
            }
            result += c;
        } else {
            result += c;
            if (c >= ' ') {
                col++;
            }
        }
    }
    return result;
}

std::string IndentText(const std::string_view text, usize num_indent_spaces) {
    if (!num_indent_spaces) return std::string {text};
    std::string spaces(num_indent_spaces, ' ');
    std::string result {spaces + std::string(text)};
    std::string replacement {"\n" + spaces};
    Replace(result, "\n", replacement);
    return result;
}

std::vector<std::string_view>
Split(std::string_view str, const std::string_view delim, bool include_empties) {
    std::vector<std::string_view> result;
    size_t pos = 0;
    while ((pos = str.find(delim)) != std::string_view::npos) {
        if (const auto section = str.substr(0, pos); section.size() || include_empties) {
            result.push_back(section);
        }
        str.remove_prefix(pos + delim.size());
    }
    if (str.size()) {
        result.push_back(str);
    }
    return result;
}

std::optional<std::string> Get3CharAlphaIdentifier(unsigned counter) {
    if (counter >= 26 * 26 * 26) {
        return {};
    }
    std::string result;
    result += (char)('a' + ((counter / (26 * 26))));
    counter -= (counter / (26 * 26)) * (26 * 26);
    result += (char)('a' + (counter / 26));
    counter -= (counter / 26) * 26;
    result += (char)('a' + (counter % 26));
    return result;
}

bool IsAbsoluteDirectory(std::string_view path) {
#if _WIN32
    // https://docs.microsoft.com/en-us/windows/win32/fileio/naming-a-file#fully-qualified-vs-relative-paths
    if (StartsWith(path, "\\\\") || StartsWith(path, "//")) return true;
    if (path.size() >= 3) return path[1] == ':' && (path[2] == '\\') || (path[2] == '/');
    return false;
#else
    return path.size() && path[0] == '/';
#endif
}

bool IsPathSyntacticallyCorrect(std::string_view path, std::string *error) {
    if (error) error->clear();
#if _WIN32
    if (IsAbsoluteDirectory(path)) {
        path = path.substr(2);
    }

    char invalid_chars[] = {'<', '>', ':', '\"', '|', '?', '*'};
    bool invalid = false;
    for (auto c : path) {
        for (auto &invalid_char : invalid_chars) {
            if (invalid_char == c) {
                invalid_char = '\0';
                invalid = true;
                if (error) error->append(1, c);
            }
        }
    }
    if (!invalid)
        error->clear();
    else {
        error->insert(0, "The path contains the following invalid characters: (");
        error->append(") ");
    }
    return !invalid;
#else
    (void)path;
    return true;
#endif
}

TEST_CASE("String Utils") {
    {
        std::string s {"th<>sef<> < seofi>"};
        REQUIRE(Replace(s, "<>", ".."));
        REQUIRE(s == "th..sef.. < seofi>");

        s = "only one";
        REQUIRE(Replace(s, "one", "two"));
        REQUIRE(s == "only two");

        REQUIRE(!Replace(s, "foo", ""));

        s = "file_c-1_C4.wav";
        REQUIRE(Replace(s, "c-1", "0"));
        REQUIRE(s == "file_0_C4.wav");

        s = "foo * * *";
        REQUIRE(Replace(s, "*", "\\*"));
        REQUIRE(s == "foo \\* \\* \\*");
    }

    {
        std::string s {"HI"};
        Lowercase(s);
        REQUIRE(s == "hi");
    }

    {
        REQUIRE(*Get3CharAlphaIdentifier(0) == "aaa");
        REQUIRE(*Get3CharAlphaIdentifier(1) == "aab");
        REQUIRE(*Get3CharAlphaIdentifier(26) == "aba");
        REQUIRE(*Get3CharAlphaIdentifier(26 * 26) == "baa");
        REQUIRE(*Get3CharAlphaIdentifier(26 * 26 * 2) == "caa");
        REQUIRE(*Get3CharAlphaIdentifier(26 * 26 * 25) == "zaa");
        REQUIRE(*Get3CharAlphaIdentifier(26 * 26 * 25 + 26 * 25) == "zza");
        REQUIRE(*Get3CharAlphaIdentifier(26 * 26 * 25 + 26 * 25 + 25) == "zzz");
        REQUIRE(!Get3CharAlphaIdentifier(26 * 26 * 25 + 26 * 25 + 26));
    }

    {
        REQUIRE(ToSnakeCase("Two Words") == "two_words");
        REQUIRE(ToCamelCase("folder name") == "FolderName");
        REQUIRE(ToCamelCase("123 what who") == "123WhatWho");
    }

    {
        const auto s = Split("hey,there,yo,", ",");
        const auto expected = {"hey", "there", "yo"};
        REQUIRE(s.size() == expected.size());
        for (usize i = 0; i < s.size(); ++i) {
            REQUIRE(s[i] == expected.begin()[i]);
        }
    }

    {
        const auto s = Split("hey\n||\nthere||yo||", "||");
        const auto expected = {"hey\n", "\nthere", "yo"};
        REQUIRE(s.size() == expected.size());
        for (usize i = 0; i < s.size(); ++i) {
            REQUIRE(s[i] == expected.begin()[i]);
        }
    }

    // test case-insensitve wildcard match
    {
        CHECK(WildcardMatch("*.WAV", "file.wav", true));
        CHECK(WildcardMatch("*.WAV", "file.WAV", true));
        CHECK(WildcardMatch("*.WAV", "file.Wav", true));
        CHECK(!WildcardMatch("*.WAV", "file.wav", false));
    }
}
