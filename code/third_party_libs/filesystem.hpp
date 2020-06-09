#pragma once

// for some reason clang and msvc are not defining this dispite having string_view available
#ifndef __cpp_lib_string_view
#define __cpp_lib_string_view
#endif

#if defined(__cplusplus) && __cplusplus >= 201703L && defined(__has_include) && !defined(__APPLE__)
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
