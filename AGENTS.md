# Repository Guidelines

## Project Structure & Module Organization
Build the SDK around a C++ core with thin Python bindings. Keep public headers in `include/audio_sdk/`, core logic in `src/`, and backend adapters under `src/backends/pipewire/`. Isolate device-control code from the real-time stream engine. Put Python bindings in `bindings/python/`, demos in `examples/`, unit tests in `tests/unit/`, hardware-in-the-loop checks in `tests/hil/`, and sample JSON configs in `configs/`.

## Build, Test, and Development Commands
Standardize on CMake plus Make:

- `mkdir build && cd build && cmake ..` configures the project.
- `make` builds the C++ library, examples, and Python module from `build/`.
- `ctest --output-on-failure` runs the default automated suite from `build/`.
- `cmake -DAUDIO_SDK_BUILD_HIL_TESTS=ON .. && make && ctest -L hil` enables and runs local hardware tests.

Keep new targets wired into CMake so contributors do not rely on ad hoc shell scripts.

## Coding Style & Naming Conventions
Use C++17 unless the repo later raises the minimum. Indent with 4 spaces, use PascalCase for public types, snake_case for functions and files, and UPPER_SNAKE_CASE for constants. Prefer RAII, explicit ownership, and small interfaces. Real-time paths must avoid blocking I/O, unbounded allocation, and Python execution; Python is a control surface, not the streaming engine. Treat strict sync as a design constraint, not a best-effort feature.

## Testing Guidelines
Use GoogleTest for C++ unit tests. Mirror source paths in tests, for example `src/stream/session.cpp` with `tests/unit/stream/test_session.cpp`. Add coverage for device discovery, routing policy, fallback selection, and config parsing. Reserve `tests/hil/` for mic, speaker, hot-plug, and record-stop-playback scenarios on Ubuntu 22.04.

## Commit & Pull Request Guidelines
Write imperative commit subjects such as `Add PipeWire device enumerator` or `Implement WAV recording lifecycle`. Pull requests should describe the behavior change, list exact verification commands, note device assumptions, and include logs or screenshots when debugging UX or diagnostics output.

Use `main` as the stable branch and do development on `feature/<topic>` branches. Tag releases from `main` with annotated semantic versions such as `v0.1.0`. Do not move published release tags.

## Configuration, Logging, and Backends
The public API should accept a JSON config path plus an optional writable state path. Recommended pattern: checked-in defaults in the consuming app repo, with runtime overrides outside source control. Do not hardcode device IDs in committed files. Treat backend selection as an internal abstraction and avoid backend-specific timing assumptions in the public API.
