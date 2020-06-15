#pragma once

// for some reason apple clang is not defining this dispite having string_view available
#ifdef __APPLE__
#ifndef __cpp_lib_string_view
#define __cpp_lib_string_view
#endif
#endif

#ifdef _WIN32
#define GHC_USE_STD_FS
#include <filesystem>
namespace fs = std::filesystem;
#endif

#ifndef GHC_USE_STD_FS
#include "filesystem_ghc.hpp"
namespace fs = ghc::filesystem;
#endif
