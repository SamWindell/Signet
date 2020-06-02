cmake_minimum_required(VERSION 3.15.0)
project(AudioUtils)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(
    flac STATIC
    code/third_party_libs/FLAC/src/bitmath.c
    code/third_party_libs/FLAC/src/bitreader.c
    code/third_party_libs/FLAC/src/bitwriter.c
    code/third_party_libs/FLAC/src/cpu.c
    code/third_party_libs/FLAC/src/crc.c
    code/third_party_libs/FLAC/src/fixed.c
    code/third_party_libs/FLAC/src/float.c
    code/third_party_libs/FLAC/src/format.c
    code/third_party_libs/FLAC/src/lpc.c
    code/third_party_libs/FLAC/src/md5.c
    code/third_party_libs/FLAC/src/memory.c
    code/third_party_libs/FLAC/src/stream_decoder.c
    code/third_party_libs/FLAC/src/stream_encoder.c
    code/third_party_libs/FLAC/src/stream_encoder_framing.c
    code/third_party_libs/FLAC/src/window.c)

if (WIN32)
    target_sources(flac PRIVATE code/third_party_libs/FLAC/src/win_utf8_io.c)
else(APPLE)
    target_compile_definitions(flac PRIVATE HAVE_LROUND)
endif ()

target_include_directories(flac PUBLIC code/third_party_libs/FLAC/src/include code/third_party_libs)
target_compile_definitions(flac PUBLIC FLAC__NO_DLL)

#

add_library(
    common OBJECT
    code/common/audio_duration.cpp
    code/common/audio_file.cpp
    code/common/common.cpp
    code/signet/backup.cpp
    code/signet/signet_interface.cpp
    code/signet/subcommands/converter/converter.cpp
    code/signet/subcommands/fader/fader.cpp
    code/signet/subcommands/normalise/gain_calculators.cpp
    code/signet/subcommands/normalise/normaliser.cpp
    code/signet/subcommands/zcross_offsetter/zcross_offsetter.cpp
    code/tests/test_helpers.cpp)
target_include_directories(common PUBLIC code/third_party_libs code/common code/tests code/signet)
target_compile_definitions(common PUBLIC NOMINMAX DOCTEST_CONFIG_SUPER_FAST_ASSERTS)
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
else ()
    target_compile_options(common PUBLIC -fdiagnostics-absolute-paths -Wall)
endif ()
target_link_libraries(common PRIVATE flac)

configure_file(${PROJECT_SOURCE_DIR}/code/tests/tests_config.h.in
               ${PROJECT_SOURCE_DIR}/code/tests/tests_config.h)

# Signet
add_executable(signet code/signet/signet_main.cpp)
target_link_libraries(signet PRIVATE common)

# Tests
add_executable(tests code/tests/tests_main.cpp)
target_link_libraries(tests PRIVATE common)
