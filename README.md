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
- `examples/`: runnable demos
- `tests/unit/`: default automated tests
- `tests/hil/`: hardware-in-the-loop tests
- `configs/`: sample config files
- `AGENTS.md`: contributor guide

## Dependencies

Required:

- CMake 3.22+
- `make`
- `pkg-config`
- PipeWire development files, typically `libpipewire-0.3-dev` on Ubuntu

Optional:

- Python3 development headers and `pybind11` for future Python bindings

## Build

Fresh local build:

```bash
mkdir build
cd build
cmake ..
make
ctest --output-on-failure
```

Enable hardware-in-the-loop tests:

```bash
mkdir build
cd build
cmake -DAUDIO_SDK_BUILD_HIL_TESTS=ON ..
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

Run the demo:

```bash
mkdir build
cd build
cmake ..
make audio_sdk_record_playback_demo
./audio_sdk_record_playback_demo
```

Run the host HIL test:

```bash
mkdir build
cd build
cmake -DAUDIO_SDK_BUILD_HIL_TESTS=ON ..
make audio_sdk_hil_record_playback
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

- `main` is the stable integration branch.
- use `feature/<topic>` branches for new work
- tag releases from `main` with annotated semantic versions such as `v0.1.0`

Contributor-specific conventions live in [AGENTS.md](/home/chiefs/audio_sdk/AGENTS.md).
