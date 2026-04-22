cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0094)
    cmake_policy(SET CMP0094 NEW)
endif()

project(audio_sdk VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AUDIO_SDK_BUILD_TESTS "Build audio_sdk tests" ON)
option(AUDIO_SDK_BUILD_HIL_TESTS "Build hardware-in-the-loop tests" OFF)
option(BUILD_EXAMPLES "Build audio_sdk examples" ON)
option(BUILD_PYTHON_BINDINGS "Build and register Python bindings" OFF)
set(PYTHON_EXECUTABLE "" CACHE FILEPATH "Optional Python executable override for bindings")

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)

add_library(audio_sdk
    src/audio_manager.cpp
    src/audio_session.cpp
    src/config.cpp
    src/backends/pipewire/pipewire_backend.cpp
)

add_library(audio_sdk::audio_sdk ALIAS audio_sdk)

set_target_properties(audio_sdk PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(audio_sdk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(audio_sdk
    PRIVATE
        PkgConfig::PIPEWIRE
)

target_compile_features(audio_sdk PUBLIC cxx_std_17)

if(BUILD_EXAMPLES)
    function(add_audio_sdk_example target source)
        add_executable(${target} ${source})
        target_link_libraries(${target} PRIVATE audio_sdk::audio_sdk)
    endfunction()

    add_audio_sdk_example(audio_sdk_example_enumerate_devices examples/cpp/01_enumerate_devices.cpp)
    add_audio_sdk_example(audio_sdk_example_config_file examples/cpp/02_config_file.cpp)
    add_audio_sdk_example(audio_sdk_example_select_devices examples/cpp/03_select_devices.cpp)
    add_audio_sdk_example(audio_sdk_example_events examples/cpp/04_events.cpp)
    add_audio_sdk_example(audio_sdk_example_recording examples/cpp/05_recording.cpp)
    add_audio_sdk_example(audio_sdk_example_playback_and_delete examples/cpp/06_playback_and_delete.cpp)
    add_audio_sdk_example(audio_sdk_example_sessions examples/cpp/07_sessions.cpp)
    add_audio_sdk_example(audio_sdk_example_fallback_policy examples/cpp/08_fallback_policy.cpp)
    add_audio_sdk_example(audio_sdk_record_playback_demo examples/cpp/07_sessions.cpp)

    add_custom_target(audio_sdk_examples
        DEPENDS
            audio_sdk_example_enumerate_devices
            audio_sdk_example_config_file
            audio_sdk_example_select_devices
            audio_sdk_example_events
            audio_sdk_example_recording
            audio_sdk_example_playback_and_delete
            audio_sdk_example_sessions
            audio_sdk_example_fallback_policy
            audio_sdk_record_playback_demo
    )
endif()

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

if(BUILD_PYTHON_BINDINGS)
    set(Python3_FIND_STRATEGY LOCATION)
    set(Python3_FIND_VIRTUALENV FIRST)

    if(PYTHON_EXECUTABLE)
        set(Python3_EXECUTABLE "${PYTHON_EXECUTABLE}" CACHE FILEPATH "Python3 executable" FORCE)
    else()
        find_program(AUDIO_SDK_PYTHON_CANDIDATE NAMES python NO_CACHE)
        if(AUDIO_SDK_PYTHON_CANDIDATE)
            execute_process(
                COMMAND "${AUDIO_SDK_PYTHON_CANDIDATE}" -c "import sys; raise SystemExit(0 if sys.version_info[0] == 3 else 1)"
                RESULT_VARIABLE AUDIO_SDK_PYTHON_CANDIDATE_IS_PY3
                OUTPUT_QUIET
                ERROR_QUIET
            )
            if(NOT AUDIO_SDK_PYTHON_CANDIDATE_IS_PY3 EQUAL 0)
                unset(AUDIO_SDK_PYTHON_CANDIDATE)
            endif()
        endif()

        if(NOT AUDIO_SDK_PYTHON_CANDIDATE)
            find_program(AUDIO_SDK_PYTHON_CANDIDATE NAMES python3 NO_CACHE REQUIRED)
        endif()

        set(Python3_EXECUTABLE "${AUDIO_SDK_PYTHON_CANDIDATE}" CACHE FILEPATH "Python3 executable selected from active PATH" FORCE)
    endif()

    find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)
    find_package(pybind11 CONFIG REQUIRED)

    set(AUDIO_SDK_PYTHON_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/audio_sdk")
    set(AUDIO_SDK_PYTHON_EDITABLE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/audio_sdk_python_editable.stamp")
    file(MAKE_DIRECTORY "${AUDIO_SDK_PYTHON_PACKAGE_DIR}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/audio_sdk/__init__.py"
        "${AUDIO_SDK_PYTHON_PACKAGE_DIR}/__init__.py"
        COPYONLY
    )

    pybind11_add_module(_audio_sdk
        bindings/python/module.cpp
    )

    set_target_properties(_audio_sdk PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${AUDIO_SDK_PYTHON_PACKAGE_DIR}"
    )
    target_link_libraries(_audio_sdk PRIVATE audio_sdk::audio_sdk)
    target_compile_definitions(_audio_sdk PRIVATE AUDIO_SDK_PYTHON_BINDINGS=1)
    add_custom_command(
        TARGET _audio_sdk
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/audio_sdk/__init__.py"
            "${AUDIO_SDK_PYTHON_PACKAGE_DIR}/__init__.py"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:_audio_sdk>"
            "${CMAKE_CURRENT_SOURCE_DIR}/audio_sdk/$<TARGET_FILE_NAME:_audio_sdk>"
    )
    add_custom_command(
        OUTPUT "${AUDIO_SDK_PYTHON_EDITABLE_STAMP}"
        COMMAND ${Python3_EXECUTABLE} -m pip install --editable "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E touch "${AUDIO_SDK_PYTHON_EDITABLE_STAMP}"
        DEPENDS
            _audio_sdk
            "${CMAKE_CURRENT_SOURCE_DIR}/setup.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/audio_sdk/__init__.py"
        COMMENT "Registering audio_sdk Python package in editable mode"
        VERBATIM
    )
    add_custom_target(audio_sdk_python_package ALL
        DEPENDS "${AUDIO_SDK_PYTHON_EDITABLE_STAMP}"
    )
    message(STATUS "Python bindings enabled")
    message(STATUS "Python executable: ${Python3_EXECUTABLE}")
else()
    message(STATUS "Python bindings disabled; enable with -DBUILD_PYTHON_BINDINGS=true")
endif()
