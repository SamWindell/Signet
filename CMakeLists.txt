cmake_minimum_required(VERSION 3.16.0)
project(Signet)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 3rd party libs
add_library(
    third_party_libs STATIC
    code/third_party_libs/dywapitchtrack/dywapitchtrack.c
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
    code/third_party_libs/FLAC/src/metadata_object.c
    code/third_party_libs/FLAC/src/stream_decoder.c
    code/third_party_libs/FLAC/src/stream_encoder.c
    code/third_party_libs/FLAC/src/stream_encoder_framing.c
    code/third_party_libs/FLAC/src/window.c
    code/third_party_libs/r8brain-resampler/r8bbase.cpp)

if (WIN32)
    target_sources(third_party_libs PRIVATE code/third_party_libs/FLAC/src/win_utf8_io.c)
else ()
    target_compile_definitions(third_party_libs PRIVATE HAVE_LROUND)
    if (NOT APPLE)
        target_link_libraries(third_party_libs PRIVATE pthread)
    endif ()
endif ()

target_include_directories(third_party_libs PUBLIC code/third_party_libs/FLAC/src/include
                                                   code/third_party_libs)
target_compile_definitions(third_party_libs PUBLIC FLAC__NO_DLL)

add_subdirectory(code/third_party_libs/cereal)
add_subdirectory(code/third_party_libs/fmt)
target_link_libraries(third_party_libs PUBLIC cereal fmt::fmt)

# Common
add_library(
    common OBJECT
    code/common/audio_data.cpp
    code/common/audio_duration.cpp
    code/common/audio_file_io.cpp
    code/common/audio_files.cpp
    code/common/backup.cpp
    code/common/common.cpp
    code/common/filepath_set.cpp
    code/common/filter.cpp
    code/common/gain_calculators.cpp
    code/common/midi_pitches.cpp
    code/common/string_utils.cpp
    code/signet/signet_interface.cpp
    code/signet/commands/auto_tune/auto_tune.cpp
    code/signet/commands/convert/convert.cpp
    code/signet/commands/fade/fade.cpp
    code/signet/commands/folderise/folderise.cpp
    code/signet/commands/gain/gain.cpp
    code/signet/commands/print_info/print_info.cpp
    code/signet/commands/move/move.cpp
    code/signet/commands/normalise/normalise.cpp
    code/signet/commands/detect_pitch/detect_pitch.cpp
    code/signet/commands/rename/rename.cpp
    code/signet/commands/rename/auto_mapper.cpp
    code/signet/commands/rename/rename_substitutions.cpp
    code/signet/commands/rename/note_to_midi.cpp
    code/signet/commands/sample_blend/sample_blend.cpp
    code/signet/commands/seamless_loop/seamless_loop.cpp
    code/signet/commands/remove_silence/remove_silence.cpp
    code/signet/commands/trim/trim.cpp
    code/signet/commands/tune/tune.cpp
    code/signet/commands/filter/filters.cpp
    code/signet/commands/zcross_offset/zcross_offset.cpp
    code/signet/commands/embed_sampler_info/embed_sampler_info.cpp
    code/tests/test_helpers.cpp)

target_include_directories(common PUBLIC code/third_party_libs code/common code/tests code/signet)
target_compile_definitions(common PUBLIC NOMINMAX DOCTEST_CONFIG_SUPER_FAST_ASSERTS)
target_precompile_headers(
    common
    PRIVATE
    "code/third_party_libs/filesystem.hpp"
    "code/third_party_libs/CLI11.hpp"
    "code/third_party_libs/json.hpp"
    <string>
    <map>
    <optional>
    <algorithm>
    <iostream>
    <vector>
    <functional>
    <array>)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(common PUBLIC SIGNET_DEBUG)
endif ()

if (MSVC)
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
        /wd4324
        /wd4127)
    target_compile_options(common PUBLIC /diagnostics:column /FC
                                         ${WARNINGS_TO_DISABLE} ${WARNINGS_TO_ENABLE})
else ()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(common PUBLIC -fdiagnostics-absolute-paths)
    endif ()
    target_compile_options(common PUBLIC -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual 
                           -Wno-switch -Wno-unused-variable)
endif ()
target_link_libraries(common PUBLIC third_party_libs)

# Tests config header
configure_file(${PROJECT_SOURCE_DIR}/code/tests/tests_config.h.in
               ${PROJECT_SOURCE_DIR}/code/tests/tests_config.h)

# Signet
add_executable(signet code/signet/signet_main.cpp)
target_link_libraries(signet PRIVATE common)
if (MSVC)
    target_sources(signet PRIVATE signet_win32.manifest)
    set_property(
        TARGET signet
        APPEND_STRING
        PROPERTY LINK_FLAGS " /MANIFESTUAC:NO")
endif ()
add_custom_command(
    TARGET signet
    POST_BUILD
    COMMAND signet make-docs ${PROJECT_SOURCE_DIR}/docs/usage.md)

# Tests
add_executable(tests code/tests/tests_main.cpp)
target_link_libraries(tests PRIVATE common)

enable_testing()
add_test(NAME tests COMMAND tests)
