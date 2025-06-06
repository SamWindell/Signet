cmake_minimum_required(VERSION 3.16.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_ENABLE_EXPORTS ON)
set(CMAKE_OSX_ARCHITECTURES
    "x86_64;arm64"
    CACHE STRING "Build universal binary")

project(Signet VERSION 0.2.0)

option(DEPLOYMENT_BUILD "A build for deployment to end-users" NO)
option(ENABLE_SANITIZERS "Enable ASan and UBSan" OFF)

if (DEPLOYMENT_BUILD)
    add_definitions(-DDOCTEST_CONFIG_DISABLE)
endif ()

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

add_subdirectory(code/third_party_libs/cereal EXCLUDE_FROM_ALL)
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
    code/common/drwav_tests.cpp
    code/common/expected_midi_pitch.cpp
    code/common/filepath_set.cpp
    code/common/filter.cpp
    code/common/gain_calculators.cpp
    code/common/identical_processing_set.cpp
    code/common/midi_pitches.cpp
    code/common/string_utils.cpp
    code/signet/commands/add_loop/add_loop.cpp
    code/signet/commands/auto_tune/auto_tune.cpp
    code/signet/commands/convert/convert.cpp
    code/signet/commands/detect_pitch/detect_pitch.cpp
    code/signet/commands/embed_sampler_info/embed_sampler_info.cpp
    code/signet/commands/fade/fade.cpp
    code/signet/commands/filter/filters.cpp
    code/signet/commands/fix_pitch_drift/fix_pitch_drift_command.cpp
    code/signet/commands/fix_pitch_drift/pitch_drift_corrector.cpp
    code/signet/commands/folderise/folderise.cpp
    code/signet/commands/gain/gain.cpp
    code/signet/commands/move/move.cpp
    code/signet/commands/normalise/normalise.cpp
    code/signet/commands/pan/pan.cpp
    code/signet/commands/print_info/print_info.cpp
    code/signet/commands/rename/auto_mapper.cpp
    code/signet/commands/rename/note_to_midi.cpp
    code/signet/commands/rename/rename.cpp
    code/signet/commands/rename/rename_substitutions.cpp
    code/signet/commands/reverse/reverse.cpp
    code/signet/commands/sample_blend/sample_blend.cpp
    code/signet/commands/seamless_loop/seamless_loop.cpp
    code/signet/commands/trim/trim.cpp
    code/signet/commands/trim_silence/trim_silence.cpp
    code/signet/commands/tune/tune.cpp
    code/signet/commands/zcross_offset/zcross_offset.cpp
    code/signet/signet_interface.cpp
    code/tests/test_helpers.cpp
    code/third_party_libs/backward.cpp)

target_include_directories(common PUBLIC code/third_party_libs code/common code/tests code/signet)
target_compile_definitions(common PUBLIC NOMINMAX DOCTEST_CONFIG_SUPER_FAST_ASSERTS)

# set_target_properties(common PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS YES)
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
    target_compile_options(common PUBLIC /diagnostics:column /FC ${WARNINGS_TO_DISABLE} ${WARNINGS_TO_ENABLE})
    target_compile_definitions(common PUBLIC WINVER=0x0601 _WIN32_WINNT=0x0601)
else ()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(common PUBLIC -fdiagnostics-absolute-paths)
    endif ()
    target_compile_options(common PUBLIC -Wall -Wextra -Wnon-virtual-dtor -Woverloaded-virtual -Wno-switch
                                         -Wno-unused-variable)
    # It would be good to enable -Wpedantic -Wshadow too, but some of the library headers have warnings

    if (ENABLE_SANITIZERS)
        set(SANITIZERS -fsanitize=address -fsanitize=undefined)
        target_compile_options(common PUBLIC ${SANITIZERS})
        target_link_options(common PUBLIC ${SANITIZERS})
    endif ()
endif ()
target_link_libraries(common PUBLIC third_party_libs)

# Tests config header
configure_file(${PROJECT_SOURCE_DIR}/code/tests/tests_config.h.in
               ${PROJECT_SOURCE_DIR}/code/tests/tests_config.h)

configure_file(${PROJECT_SOURCE_DIR}/code/signet/version.h.in ${PROJECT_SOURCE_DIR}/code/signet/version.h)

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

install(TARGETS signet RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Tests
add_executable(tests code/tests/tests_main.cpp)
target_link_libraries(tests PRIVATE common)

enable_testing()
add_test(NAME tests COMMAND tests)
