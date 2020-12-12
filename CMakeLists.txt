cmake_minimum_required(VERSION 3.15.0)
project(Signet)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(
    third_party_libs STATIC
    code/third_party_libs/dywapitchtrack/dywapitchtrack.c
    code/third_party_libs/flac/src/bitmath.c
    code/third_party_libs/flac/src/bitreader.c
    code/third_party_libs/flac/src/bitwriter.c
    code/third_party_libs/flac/src/cpu.c
    code/third_party_libs/flac/src/crc.c
    code/third_party_libs/flac/src/fixed.c
    code/third_party_libs/flac/src/float.c
    code/third_party_libs/flac/src/format.c
    code/third_party_libs/flac/src/lpc.c
    code/third_party_libs/flac/src/md5.c
    code/third_party_libs/flac/src/memory.c
    code/third_party_libs/flac/src/stream_decoder.c
    code/third_party_libs/flac/src/stream_encoder.c
    code/third_party_libs/flac/src/stream_encoder_framing.c
    code/third_party_libs/flac/src/window.c
    code/third_party_libs/flac/src/metadata_object.c
    code/third_party_libs/r8brain-resampler/r8bbase.cpp)

if (WIN32)
    target_sources(third_party_libs PRIVATE code/third_party_libs/flac/src/win_utf8_io.c)
else (APPLE)
    target_compile_definitions(third_party_libs PRIVATE HAVE_LROUND)
endif ()

target_include_directories(third_party_libs PUBLIC code/third_party_libs/flac/src/include
                                                   code/third_party_libs)
target_compile_definitions(third_party_libs PUBLIC FLAC__NO_DLL)

add_subdirectory(code/third_party_libs/cereal)
target_link_libraries(third_party_libs PUBLIC cereal)

#

add_library(
    common OBJECT
    code/common/audio_duration.cpp
    code/common/audio_file.cpp
    code/common/backup.cpp
    code/common/filter.cpp
    code/common/common.cpp
    code/common/input_files.cpp
    code/common/midi_pitches.cpp
    code/common/pathname_expansion.cpp
    code/common/string_utils.cpp
    code/signet/signet_interface.cpp
    code/signet/subcommands/auto_tuner/auto_tuner.cpp
    code/signet/subcommands/converter/converter.cpp
    code/signet/subcommands/fader/fader.cpp
    code/signet/subcommands/folderiser/folderiser.cpp
    code/signet/subcommands/gainer/gainer.cpp
    code/signet/subcommands/normaliser/gain_calculators.cpp
    code/signet/subcommands/normaliser/normaliser.cpp
    code/signet/subcommands/pitch_detector/pitch_detector.cpp
    code/signet/subcommands/renamer/renamer.cpp
    code/signet/subcommands/renamer/auto_mapper.cpp
    code/signet/subcommands/renamer/rename_substitutions.cpp
    code/signet/subcommands/renamer/note_to_midi.cpp
    code/signet/subcommands/sample_blender/sample_blender.cpp
    code/signet/subcommands/silence_remover/silence_remover.cpp
    code/signet/subcommands/trimmer/trimmer.cpp
    code/signet/subcommands/tuner/tuner.cpp
    code/signet/subcommands/filters/filters.cpp
    code/signet/subcommands/zcross_offsetter/zcross_offsetter.cpp
    code/tests/test_helpers.cpp)

target_include_directories(common PUBLIC code/third_party_libs code/common code/tests code/signet)
target_compile_definitions(common PUBLIC NOMINMAX DOCTEST_CONFIG_SUPER_FAST_ASSERTS DRWAV_MAX_SMPL_LOOPS=16)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(common PUBLIC SIGNET_DEBUG)
endif()

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
    target_compile_options(common PUBLIC -fdiagnostics-absolute-paths -Wall)
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
endif()

# Tests
add_executable(tests code/tests/tests_main.cpp)
target_link_libraries(tests PRIVATE common)
