#pragma once

// for some reason apple clang is not defining this dispite having string_view available
#ifdef __APPLE__
#ifndef __cpp_lib_string_view
#define __cpp_lib_string_view
#endif
#endif

#ifdef _WIN32
#include <filesystem>
namespace fs = std::filesystem;
#else
#include "filesystem_ghc.hpp"
namespace fs = ghc::filesystem;
#endif
