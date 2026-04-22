# Changelog

All notable changes to `audio_sdk` must be documented in this file.

## Unreleased

### Added

- Added organized C++ examples under `examples/cpp/` for device enumeration, config load/save, manual device selection, events, recording, playback/delete, session lifecycle, and fallback policy.
- Added organized Python examples under `examples/python/` covering the same SDK feature set as the C++ examples.
- Added `examples/README.md` with build and run instructions for C++ and Python examples.
- Added Python package wrapper `audio_sdk` so users can write `import audio_sdk` instead of importing `_audio_sdk` directly.
- Added editable Python package registration during `make` when `BUILD_PYTHON_BINDINGS=true`.
- Added `setup.py` for local editable Python package registration.
- Added CMake flags:
  - `BUILD_PYTHON_BINDINGS`
  - `PYTHON_EXECUTABLE`
  - `BUILD_EXAMPLES`
- Added short, SDK-managed PipeWire device IDs such as `in-12ab34cd56` and `out-12ab34cd56`.
- Added automatic Python interpreter detection for binding builds, including activated conda and venv environments.

### Changed

- Standardized the documented build process around `cmake ..` and `make`, with feature selection handled by CMake flags.
- Changed Python binding builds so plain `make` builds `_audio_sdk` and registers the editable `audio_sdk` package when `BUILD_PYTHON_BINDINGS=true`.
- Changed `PYTHON_EXECUTABLE` from a normally required build argument into an optional override; when omitted, CMake selects the active Python 3 interpreter from `PATH`.
- Changed C++ examples from one flat demo file to a grouped example suite.
- Kept `audio_sdk_record_playback_demo` as a compatibility build target backed by the session example.
- Expanded Python bindings to expose device descriptors, event types, session objects, event callbacks, and manager config access.
- Made the core static library position-independent so it can link into the Python extension module.
- Documented that short SDK device IDs are stable when the backend exposes stable properties, with a boot-local fallback for unusual virtual nodes.

### Removed

- Removed the flat `examples/record_playback_demo.cpp` file after replacing it with the organized C++ example suite.

## v0.1.0 - 2026-04-18

### Added

- Added the first PipeWire-backed C++ SDK release.
- Added device enumeration through PipeWire.
- Added config-driven input and output route selection by device ID or device name.
- Added fallback route policy for future streams when a selected device disappears.
- Added WAV recording, stop, playback, and delete flows.
- Added long-lived recording and playback sessions through `AudioSession`.
- Added C++ event callbacks for device, route, stream, fallback, recording saved, and recording deleted events.
- Added CMake build for the C++ library, examples, tests, and optional Python bindings.
- Added unit smoke tests and hardware-in-the-loop record/playback validation.

### Known Limitations

- Active streams are not live-rebound to fallback devices yet.
- Mic-to-speaker forwarding and arbitrary live stream graph routing are not implemented yet.
- Device capability inspection is limited to basic identity, direction, default, and availability fields.
- JSON parsing is intentionally narrow and temporary.
- Python bindings are still a control-plane layer and not yet a stable full API.
