# Repository Guidelines

## Project Structure & Module Organization
Build the SDK around a C++ core with thin Python bindings. Keep public headers in `include/audio_sdk/`, core logic in `src/`, and backend adapters under `src/backends/` with `pipewire/` as the primary streaming backend. Isolate device-control code from the real-time stream engine so future compatibility layers can be added without changing the public API. Put Python bindings in `bindings/python/`, demos in `examples/`, unit tests in `tests/unit/`, and hardware-in-the-loop checks in `tests/hil/`. Store sample JSON configs in `configs/`; consuming applications should pass their own config file path into the SDK.

## Build, Test, and Development Commands
Standardize on CMake plus Make:

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` configures the project.
- `make -C build -j` builds the C++ library, examples, and Python module.
- `ctest --test-dir build --output-on-failure` runs the default automated suite.
- `ctest --test-dir build -L hil` runs local hardware tests against real devices.

Keep new targets wired into CMake so contributors do not rely on ad hoc shell scripts.

## Coding Style & Naming Conventions
Use C++17 unless the repo later raises the minimum. Indent with 4 spaces, use PascalCase for public types, snake_case for functions and files, and UPPER_SNAKE_CASE for constants. Prefer RAII, explicit ownership, and small interfaces. Real-time paths must avoid blocking I/O, unbounded allocation, and Python execution; Python is a control surface, not the primary streaming engine. Treat strict sync as a design constraint, not a best-effort feature.

## Testing Guidelines
Use GoogleTest for C++ unit tests; it is open source and suitable here. Mirror source paths in tests, for example `src/stream/session.cpp` with `tests/unit/stream/test_session.cpp`. Add unit coverage for device discovery, routing policy, fallback selection, and config parsing. Reserve `tests/hil/` for mic, speaker, hot-plug, and record-stop-playback scenarios on Ubuntu 22.04.

## Commit & Pull Request Guidelines
Write short imperative commit subjects such as `Add PipeWire device enumerator` or `Implement WAV recording lifecycle`. Pull requests should describe the behavior change, list exact verification commands, note device assumptions, and include logs or screenshots when debugging UX or diagnostics output.

## Configuration, Logging, and Backends
The public API should accept a JSON config path plus an optional writable state path. Recommended pattern: checked-in defaults in the consuming app repo, with runtime overrides outside source control. Do not hardcode machine-specific device IDs in committed files. Treat backend selection as an internal abstraction and avoid backend-specific timing assumptions in the public API.
