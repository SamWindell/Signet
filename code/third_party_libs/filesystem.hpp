#pragma once

// for some reason apple clang is not defining this dispite having string_view available
#if defined(__APPLE__)
#ifndef __cpp_lib_string_view
#define __cpp_lib_string_view
#endif
#endif

#if defined(__cplusplus) && __cplusplus >= 201703L && defined(__has_include) && defined(_WIN32)
#if __has_include(<filesystem>)
#define GHC_USE_STD_FS
#include <filesystem>
namespace fs = std::filesystem;
#endif
#endif
#ifndef GHC_USE_STD_FS
#include "filesystem_ghc.hpp"
namespace fs = ghc::filesystem;
#endif
