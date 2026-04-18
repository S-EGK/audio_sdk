cmake_minimum_required(VERSION 3.22)

project(audio_sdk VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AUDIO_SDK_BUILD_TESTS "Build audio_sdk tests" ON)
option(AUDIO_SDK_BUILD_HIL_TESTS "Build hardware-in-the-loop tests" OFF)
option(AUDIO_SDK_BUILD_PYTHON "Build Python bindings when pybind11 is available" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)

add_library(audio_sdk
    src/audio_manager.cpp
    src/config.cpp
    src/backends/pipewire/pipewire_backend.cpp
)

add_library(audio_sdk::audio_sdk ALIAS audio_sdk)

target_include_directories(audio_sdk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(audio_sdk
    PRIVATE
        PkgConfig::PIPEWIRE
)

target_compile_features(audio_sdk PUBLIC cxx_std_17)

add_executable(audio_sdk_record_playback_demo
    examples/record_playback_demo.cpp
)

target_link_libraries(audio_sdk_record_playback_demo PRIVATE audio_sdk::audio_sdk)

if(AUDIO_SDK_BUILD_TESTS)
    enable_testing()

    add_executable(audio_sdk_smoke_tests
        tests/unit/test_audio_manager.cpp
    )

    target_link_libraries(audio_sdk_smoke_tests PRIVATE audio_sdk::audio_sdk)

    add_test(NAME audio_sdk_smoke_tests COMMAND audio_sdk_smoke_tests)

    if(AUDIO_SDK_BUILD_HIL_TESTS)
        add_executable(audio_sdk_hil_record_playback
            tests/hil/test_record_playback_hil.cpp
        )

        target_link_libraries(audio_sdk_hil_record_playback PRIVATE audio_sdk::audio_sdk)

        add_test(NAME audio_sdk_hil_record_playback COMMAND audio_sdk_hil_record_playback)
        set_tests_properties(audio_sdk_hil_record_playback PROPERTIES LABELS hil)
    endif()
endif()

if(AUDIO_SDK_BUILD_PYTHON)
    find_package(Python3 COMPONENTS Interpreter Development.Module QUIET)
    find_package(pybind11 CONFIG QUIET)

    if(Python3_FOUND AND pybind11_FOUND)
        pybind11_add_module(_audio_sdk
            bindings/python/module.cpp
        )

        target_link_libraries(_audio_sdk PRIVATE audio_sdk::audio_sdk)
        target_compile_definitions(_audio_sdk PRIVATE AUDIO_SDK_PYTHON_BINDINGS=1)
    else()
        message(STATUS "Python bindings disabled: Python3 and/or pybind11 not found")
    endif()
endif()

