cmake_minimum_required(VERSION 3.15.0)
project(AudioUtils)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(common STATIC code/common/audio_file.cpp code/common/audio_duration.cpp)
target_include_directories(common PUBLIC code/third_party_libs code/common code/tests code/signet)
target_compile_definitions(common PUBLIC NOMINMAX)
if (WIN32)
    set(WARNINGS_TO_ENABLE
        /W4
        /w14242
        /w14263
        /w14296
        /w14928
        /w14265
        /w14266)
    set(WARNINGS_TO_DISABLE
        /wd4505
        /wd4100
        /wd4201
        /wd4189
        /wd5054
        /wd4702
        /wd4324)
    set(FULL_PATH_OF_SOURCE_CODE "/FC")
    target_compile_options(common PUBLIC /diagnostics:column ${FULL_PATH_OF_SOURCE_CODE}
                                                    ${WARNINGS_TO_DISABLE} ${WARNINGS_TO_ENABLE})
endif ()

add_library(subcommands STATIC code/signet/subcommands/fader/fader.cpp
               code/signet/subcommands/normalise/gain_calculators.cpp
               code/signet/subcommands/normalise/normaliser.cpp)
target_link_libraries(subcommands PUBLIC common)

# Signet
add_executable(signet code/signet/signet_main.cpp code/signet/signet_interface.cpp)
target_link_libraries(signet PRIVATE subcommands)

# Tests
add_executable(tests code/tests/tests_main)
target_link_libraries(tests PRIVATE subcommands)

