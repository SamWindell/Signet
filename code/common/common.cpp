#include "common.h"

#include "filesystem.hpp"

bool PatternMatch(std::string_view pattern, std::string_view name) {
    size_t px = 0;
    size_t nx = 0;
    size_t next_px = 0;
    size_t next_nx = 0;
    while (px < pattern.size() || nx < name.size()) {
        if (px < pattern.size()) {
            auto c = pattern[px];
            switch (c) {
                case '?': { // single-character wildcard
                    if (nx < name.size()) {
                        px++;
                        nx++;
                        continue;
                    }
                    break;
                }
                case '*': { // zero-or-more-character wildcard
                    // Try to match at nx.
                    // If that doesn't work out,
                    // restart at nx+1 next.
                    next_px = px;
                    next_nx = nx + 1;
                    px++;
                    continue;
                }
                default: { // ordinary character
                    if (nx < name.size() && name[nx] == c) {
                        px++;
                        nx++;
                        continue;
                    }
                    break;
                }
            }
        }
        // Mismatch. Maybe restart.
        if (0 < next_nx && next_nx <= name.size()) {
            px = next_px;
            nx = next_nx;
            continue;
        }
        return false;
    }
    // Matched all of pattern to all of name. Success.
    return true;
}

static void SafeFClose(FILE *f) {
    if (f) fclose(f);
}

std::unique_ptr<FILE, void (*)(FILE *)> OpenFile(const ghc::filesystem::path &path, const char *mode) {
#if _WIN32
    FILE *f;
    WCHAR wchar_mode[8];
    for (int i = 0; i < 7; ++i) {
        wchar_mode[i] = mode[i];
        if (mode[i] == '\0') break;
    }
    const auto ec = _wfopen_s(&f, path.wstring().data(), wchar_mode);
    if (ec == 0) {
        return {f, &SafeFClose};
    } else {
        return {nullptr, &SafeFClose};
    }
#else
    return {std::fopen(path.generic_string().data(), mode), &SafeFClose};
#endif
}
