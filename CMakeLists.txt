cmake_minimum_required(VERSION 3.15.0)
project(AudioUtils)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(audio_util_build_settings STATIC code/common/common.cpp)
target_include_directories(audio_util_build_settings
                           PUBLIC code/third_party_libs code/common)
target_compile_definitions(audio_util_build_settings PUBLIC NOMINMAX)

add_executable(offset_start_to_zero_crossing
               code/audio_utils/offset_start_to_zero_crossing/main.cpp)
target_link_libraries(offset_start_to_zero_crossing
                      PRIVATE audio_util_build_settings)

add_executable(normalise code/audio_utils/normalise/main.cpp)
target_link_libraries(normalise PRIVATE audio_util_build_settings)
