#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "filesystem.hpp"
#include "types.h"

bool EndsWith(std::string_view str, std::string_view suffix);
bool StartsWith(std::string_view str, std::string_view prefix);
bool Contains(std::string_view haystack, std::string_view needle);

bool WildcardMatch(std::string_view pattern, std::string_view name);

bool RegexReplace(std::string &str, std::string pattern, std::string replacement);
bool Replace(std::string &str, std::string_view a, std::string_view b);
bool Replace(std::string &str, char a, char b);
bool Remove(std::string &str, char c);

void Lowercase(std::string &str);
std::string ToSnakeCase(std::string_view str);
std::string ToCamelCase(std::string_view str);

std::string PutNumberInAngleBracket(usize num);
std::string WrapText(const std::string_view text, const unsigned width);
std::string IndentText(const std::string_view text, usize num_indent_spaces);

std::string GetJustFilenameWithNoExtension(fs::path path);
std::vector<std::string_view>
Split(std::string_view str, std::string_view delim, bool include_empties = false);

std::optional<std::string> Get3CharAlphaIdentifier(unsigned counter);

bool IsAbsoluteDirectory(std::string_view path);
bool IsPathSyntacticallyCorrect(std::string_view path, std::string *error = nullptr);