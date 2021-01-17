cmake_minimum_required(VERSION 3.15.0)
project(Signet)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
else (APPLE)
    target_compile_definitions(third_party_libs PRIVATE HAVE_LROUND)
endif ()

target_include_directories(third_party_libs PUBLIC code/third_party_libs/FLAC/src/include
                                                   code/third_party_libs)
target_compile_definitions(third_party_libs PUBLIC FLAC__NO_DLL)

add_subdirectory(code/third_party_libs/cereal)
add_subdirectory(code/third_party_libs/fmt)
target_link_libraries(third_party_libs PUBLIC cereal fmt::fmt)

#

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
    code/signet/subcommands/auto_tune/auto_tune.cpp
    code/signet/subcommands/convert/convert.cpp
    code/signet/subcommands/fade/fade.cpp
    code/signet/subcommands/folderise/folderise.cpp
    code/signet/subcommands/gain/gain.cpp
    code/signet/subcommands/print_info/print_info.cpp
    code/signet/subcommands/move/move.cpp
    code/signet/subcommands/normalise/normalise.cpp
    code/signet/subcommands/detect_pitch/detect_pitch.cpp
    code/signet/subcommands/rename/rename.cpp
    code/signet/subcommands/rename/auto_mapper.cpp
    code/signet/subcommands/rename/rename_substitutions.cpp
    code/signet/subcommands/rename/note_to_midi.cpp
    code/signet/subcommands/sample_blend/sample_blend.cpp
    code/signet/subcommands/seamless_loop/seamless_loop.cpp
    code/signet/subcommands/remove_silence/remove_silence.cpp
    code/signet/subcommands/trim/trim.cpp
    code/signet/subcommands/tune/tune.cpp
    code/signet/subcommands/filter/filters.cpp
    code/signet/subcommands/zcross_offset/zcross_offset.cpp
    code/signet/subcommands/embed_sampler_info/embed_sampler_info.cpp
    code/tests/test_helpers.cpp)

target_include_directories(common PUBLIC code/third_party_libs code/common code/tests code/signet)
target_compile_definitions(common PUBLIC NOMINMAX DOCTEST_CONFIG_SUPER_FAST_ASSERTS DRWAV_MAX_SMPL_LOOPS=16)
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
        /wd4324
        /wd4127)
    set(FULL_PATH_OF_SOURCE_CODE "/FC")
    target_compile_options(common PUBLIC /diagnostics:column ${FULL_PATH_OF_SOURCE_CODE}
                                         ${WARNINGS_TO_DISABLE} ${WARNINGS_TO_ENABLE})
else ()
    target_compile_options(common PUBLIC -fdiagnostics-absolute-paths -Wall -ftime-trace)
endif ()
target_link_libraries(common PUBLIC third_party_libs)

configure_file(${PROJECT_SOURCE_DIR}/code/tests/tests_config.h.in
               ${PROJECT_SOURCE_DIR}/code/tests/tests_config.h)

# Signet
add_executable(signet code/signet/signet_main.cpp)
target_link_libraries(signet PRIVATE common)
if (WIN32)
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
