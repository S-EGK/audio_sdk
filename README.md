# audio_sdk

`audio_sdk` is a Linux-first C++ audio device and stream control SDK for robotics and other low-latency applications. It provides a single high-level API for device discovery, route selection, fallback policy, WAV recording, file-backed playback, and long-lived audio session control. The current backend is PipeWire.

## Current Scope

Implemented now:

- device enumeration through PipeWire
- event callbacks for device add/remove and stream lifecycle
- config-driven input and output selection by device ID or device name
- fallback route selection when a selected device disappears
- one-shot record / stop / play / delete flows
- session-based recording and playback through `AudioSession`
- hardware-in-the-loop validation on Ubuntu 22.04

Not implemented yet:

- active-stream live rebind to fallback devices
- mic-to-speaker forwarding or arbitrary stream graph routing
- capability inspection beyond basic device identity
- a production JSON parser
- stable Python bindings

## Platform and Backend

- primary platform: Ubuntu 22.04
- language: C++17
- backend: PipeWire
- public API style: backend-neutral control surface

This repository should currently be treated as a source-built SDK, not an installed system package.

## Repository Layout

- `include/audio_sdk/`: public headers
- `src/`: SDK implementation
- `src/backends/pipewire/`: PipeWire backend
- `examples/cpp/`: runnable C++ demos
- `examples/python/`: runnable Python demos for the optional bindings
- `tests/unit/`: default automated tests
- `tests/hil/`: hardware-in-the-loop tests
- `configs/`: sample config files
- `CHANGELOG.md`: required release and change history
- `AGENTS.md`: contributor guide

## Dependencies

Required:

- CMake 3.22+
- `make`
- `pkg-config`
- PipeWire development files, typically `libpipewire-0.3-dev` on Ubuntu

Optional:

- Python3 development headers, `pip`, and `pybind11` for Python bindings

## Build

Fresh local build:

```bash
mkdir build
cd build
cmake ..
make
ctest --output-on-failure
```

Common CMake flags:

```bash
cmake .. \
    -DBUILD_PYTHON_BINDINGS=true \
    -DBUILD_EXAMPLES=true \
    -DAUDIO_SDK_BUILD_HIL_TESTS=false
make
```

Relevant flags:

- `BUILD_PYTHON_BINDINGS`: build `_audio_sdk` and register the editable `audio_sdk` Python package during `make`.
- `PYTHON_EXECUTABLE`: optional Python interpreter override. When omitted, CMake auto-detects the active Python 3 interpreter from `PATH`, so activated conda and venv environments are used automatically.
- `BUILD_EXAMPLES`: build C++ example executables as part of `make`.
- `AUDIO_SDK_BUILD_HIL_TESTS`: include hardware-in-the-loop tests.

To use a conda or venv Python, activate the environment before configuring:

```bash
conda activate audio-sdk
# or: source .venv/bin/activate
mkdir build
cd build
cmake .. -DBUILD_PYTHON_BINDINGS=true
make
```

If you need to force a specific interpreter, pass an explicit override:

```bash
cmake .. -DBUILD_PYTHON_BINDINGS=true -DPYTHON_EXECUTABLE="$(which python)"
```

If an existing `build/` directory previously cached an explicit interpreter, clear it once with `cmake .. -DPYTHON_EXECUTABLE=` or recreate the build directory.

Enable hardware-in-the-loop tests:

```bash
mkdir build
cd build
cmake -DAUDIO_SDK_BUILD_HIL_TESTS=true ..
make
ctest -L hil --output-on-failure
```

## Using the SDK in CMake

The simplest current integration model is to add this repository as a subdirectory:

```cmake
add_subdirectory(path/to/audio_sdk)
target_link_libraries(my_robot_app PRIVATE audio_sdk::audio_sdk)
```

There is no install/export packaging flow yet, so source integration is the intended path for now.

## Public API

The main entry point is `audio_sdk::AudioManager` in [include/audio_sdk/audio_manager.h](/home/chiefs/audio_sdk/include/audio_sdk/audio_manager.h:21).

Device IDs returned by `EnumerateDevices()` are SDK-managed short identifiers such as `in-12ab34cd56` or `out-12ab34cd56`. The PipeWire backend derives them from stable device properties when available, so normal hardware IDs should remain stable across reboot. If a backend cannot see stable device properties for a virtual or unusual node, that device may still fall back to a boot-local identity.

Core operations:

- `EnumerateDevices()`
- `SelectInputDevice(device_id)`
- `SelectOutputDevice(device_id)`
- `StartRecording(path)` / `StopRecording()`
- `PlayRecording(path)`
- `DeleteRecording(path)`
- `CreateRecordingSession(path)`
- `CreatePlaybackSession(path)`
- `SetEventCallback(...)`

Long-lived work is represented by `audio_sdk::AudioSession` in [include/audio_sdk/audio_session.h](/home/chiefs/audio_sdk/include/audio_sdk/audio_session.h:15).

Session operations:

- `Start()`
- `Stop()`
- `active()`
- `mode()`
- `path()`

All operations return `audio_sdk::Status`. Use `if (!status)` and inspect `status.message` on failure.

## Quick Start: Record and Play

```cpp
#include <chrono>
#include <iostream>
#include <thread>

#include "audio_sdk/audio_manager.h"

int main() {
    audio_sdk::AudioManager manager;

    const auto devices = manager.EnumerateDevices();
    for (const auto& device : devices) {
        std::cout << device.id << " -> " << device.name << '\n';
    }

    const std::string path = "recordings/sample.wav";

    auto status = manager.StartRecording(path);
    if (!status) {
        std::cerr << status.message << '\n';
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    status = manager.StopRecording();
    if (!status) {
        std::cerr << status.message << '\n';
        return 1;
    }

    status = manager.PlayRecording(path);
    if (!status) {
        std::cerr << status.message << '\n';
        return 1;
    }

    return 0;
}
```

## Session API Example

Use sessions when you want explicit lifecycle control instead of a single blocking call:

```cpp
#include <chrono>
#include <thread>

#include "audio_sdk/audio_manager.h"

int main() {
    audio_sdk::AudioManager manager;

    auto record = manager.CreateRecordingSession("recordings/session.wav");
    if (!record || !record->Start()) {
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    record->Stop();

    auto playback = manager.CreatePlaybackSession("recordings/session.wav");
    if (!playback || !playback->Start()) {
        return 1;
    }

    while (playback->active()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
```

## Configuration

`AudioManager` accepts an `AudioConfig` directly or can load one from JSON with `LoadConfigFromFile(...)`.

Current config shape is flat:

```json
{
  "backend": "pipewire",
  "input_preferred_id": "",
  "input_preferred_name": "Primary Microphone",
  "input_fallback_id": "",
  "input_fallback_name": "Backup Microphone",
  "output_preferred_id": "",
  "output_preferred_name": "Primary Speaker",
  "output_fallback_id": "",
  "output_fallback_name": "Backup Speaker",
  "recording_output_path": "recordings/last_capture.wav",
  "recording_delete_after_playback": false,
  "stream_sample_rate": 48000,
  "stream_channels": 2,
  "stream_strict_sync": true
}
```

Relevant policy fields:

- `preferred_id` / `preferred_name`: first choice
- `fallback_id` / `fallback_name`: route to use after device loss
- `stream_sample_rate`: current stream sample rate target
- `stream_channels`: currently mono or stereo
- `stream_strict_sync`: reserved for stricter future behavior

The current JSON parser is temporary and intentionally narrow. Expect the config layer to be upgraded later.

## Events

Register an event callback with `SetEventCallback(...)`.

Key events currently emitted:

- `kDeviceAdded`
- `kDeviceRemoved`
- `kRouteChanged`
- `kStreamStarted`
- `kStreamStopped`
- `kStreamError`
- `kStreamFallbackApplied`
- `kRecordingSaved`
- `kRecordingDeleted`

Example:

```cpp
manager.SetEventCallback([](const audio_sdk::Event& event) {
    std::cout << "event=" << static_cast<int>(event.type)
              << " device=" << event.device_id
              << " details=" << event.details << '\n';
});
```

## Current Behavior on Device Loss

If the selected device disappears:

- the manager resolves a fallback route from config if one is available
- future streams use that fallback route
- `kStreamFallbackApplied` and `kRouteChanged` are emitted
- an already-running stream is not live-migrated yet

That last point is the main remaining gap before a stronger robotics-grade recovery story.

## Examples and Validation

Build with examples enabled:

```bash
mkdir build
cd build
cmake -DBUILD_EXAMPLES=true ..
make
```

Run individual C++ examples from `build/`, for example:

```bash
./audio_sdk_example_enumerate_devices
./audio_sdk_example_recording
./audio_sdk_example_sessions
```

Build the optional Python module when Python development headers, `pip`, and `pybind11` are installed:

```bash
mkdir build
cd build
cmake -DBUILD_PYTHON_BINDINGS=true ..
make
```

By default, CMake uses the active Python 3 interpreter from `PATH`. Activate a conda or venv environment before running `cmake` when the package should be registered into that environment.

After `make`, the package can be imported from any working directory:

```bash
python3 -c "import audio_sdk; print(audio_sdk.__version__)"
```

Run Python examples from the repository root, for example:

```bash
python3 -m examples.python.01_enumerate_devices
python3 -m examples.python.07_sessions
```

See [examples/README.md](/home/chiefs/audio_sdk/examples/README.md) for the full example list.

Run the host HIL test:

```bash
mkdir build
cd build
cmake -DAUDIO_SDK_BUILD_HIL_TESTS=true ..
make
./audio_sdk_hil_record_playback
```

The HIL test expects a real input device and output device visible to PipeWire.

## Roadmap

Near-term priorities:

1. Rebind active sessions to fallback devices instead of only updating future route choices.
2. Add forwarding and richer live-stream control beyond file-backed playback.
3. Replace the temporary regex-based JSON parser with a proper JSON dependency.
4. Expand hardware-assisted validation and diagnostics.
5. Add Python bindings as a control-plane layer over the C++ core.

## Development Workflow

- `main` is the tested release branch.
- `develop` is the active integration branch for ongoing work.
- every change must update [CHANGELOG.md](/home/chiefs/audio_sdk/CHANGELOG.md) with detailed behavior and integration notes.
- merge `develop` to `main` only after the release candidate passes documented build, unit test, and applicable HIL validation.
- tag releases from `main` with annotated semantic versions such as `v0.1.0`

Contributor-specific conventions live in [AGENTS.md](/home/chiefs/audio_sdk/AGENTS.md).
