#pragma once
#include <string>
#include <string_view>

#include "types.h"

bool EndsWith(std::string_view str, std::string_view suffix);
bool StartsWith(std::string_view str, std::string_view prefix);
bool Contains(std::string_view haystack, std::string_view needle);

bool WildcardMatch(std::string_view pattern, std::string_view name);

bool RegexReplace(std::string &str, std::string pattern, std::string replacement);
bool Replace(std::string &str, std::string_view a, std::string_view b);

std::string PutNumberInAngleBracket(usize num);
std::string WrapText(const std::string &text, const unsigned width, const usize indent_spaces = 0);

namespace ghc {
namespace filesystem {
class path;
}
} // namespace ghc
std::string GetJustFilenameWithNoExtension(ghc::filesystem::path path);
