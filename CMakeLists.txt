cmake_minimum_required(VERSION 3.15.0)
project(AudioUtils)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(audio_util_common STATIC code/common/audio_file.cpp code/common/audio_util_interface.cpp)
target_include_directories(audio_util_common PUBLIC code/third_party_libs code/common code/tests)
target_compile_definitions(audio_util_common PUBLIC NOMINMAX)
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
    target_compile_options(audio_util_common PUBLIC /diagnostics:column ${FULL_PATH_OF_SOURCE_CODE}
                                                    ${WARNINGS_TO_DISABLE} ${WARNINGS_TO_ENABLE})
endif ()

# Zero Offsetter
add_executable(offset_start_to_zero_crossing code/audio_utils/offset_start_to_zero_crossing/main.cpp)
target_link_libraries(offset_start_to_zero_crossing PRIVATE audio_util_common)

# Normalise
add_executable(normalise code/audio_utils/normalise/gain_calculators.cpp code/audio_utils/normalise/main.cpp
                         code/audio_utils/normalise/normaliser.cpp)
target_link_libraries(normalise PRIVATE audio_util_common)

# Fader
add_executable(fader code/audio_utils/fader/main.cpp code/audio_utils/fader/fader.cpp)
target_link_libraries(fader PRIVATE audio_util_common)

# Tests
add_executable(tests code/audio_utils/normalise/normaliser.cpp code/tests/tests_main code/audio_utils/normalise/gain_calculators.cpp code/audio_utils/fader/fader.cpp code/common/audio_duration.cpp)
target_link_libraries(tests PRIVATE audio_util_common)
