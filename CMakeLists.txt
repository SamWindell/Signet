cmake_minimum_required(VERSION 3.15.0)
project(AudioUtils)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(audio_util_build_settings INTERFACE)
target_include_directories(audio_util_build_settings INTERFACE libs common)

add_executable(offset_start_to_zero_crossing
               offset_start_to_zero_crossing/main.cpp common/common.cpp)
target_link_libraries(offset_start_to_zero_crossing
                      PRIVATE audio_util_build_settings)
