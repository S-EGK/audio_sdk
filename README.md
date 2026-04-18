# audio_sdk

`audio_sdk` is a high-level, robotics-oriented audio control and streaming SDK. The project is Linux-first, written in C++, and uses PipeWire as the primary real-time backend. Python bindings are planned as a thin control-plane interface over the C++ core.

## Goals

- Provide a single public API for device discovery, routing, recording, playback, live streams, and hot-plug recovery.
- Keep the public API backend-neutral while allowing strict-sync, low-latency behavior for robotics workloads.
- Accept application-owned JSON configuration files instead of hardcoding machine-specific device settings.

## Current Status

This repository is at bootstrap stage. The initial scaffold focuses on:

- a backend-neutral C++ API shape
- a PipeWire backend boundary
- sample JSON config handling
- a record / stop / play / delete lifecycle stub
- local build and smoke-test wiring

The current implementation is intentionally thin. It compiles and exercises the expected control flow, but it does not yet perform full real-time capture/playback through PipeWire.

## Repository Layout

- `include/audio_sdk/`: public headers
- `src/`: core SDK implementation
- `src/backends/pipewire/`: PipeWire-specific backend code
- `bindings/python/`: pybind11 module source
- `examples/`: small runnable demos
- `tests/unit/`: default automated tests
- `tests/hil/`: hardware-in-the-loop tests for real devices
- `configs/`: sample JSON configs
- `docs/`: architecture notes and design decisions

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
make -C build -j
ctest --test-dir build --output-on-failure
```

Python bindings are built only when both `Python3` and `pybind11` are installed.

## Recommended Direction

- Use PipeWire for real-time graph and strict-sync stream work.
- Keep Python out of real-time callbacks.
- Store checked-in app defaults in the consuming app repo and write runtime overrides to an external state file.
- Add deeper backend features incrementally behind the same public API.

## Near-Term Roadmap

1. Implement real device enumeration and event subscriptions through PipeWire.
2. Replace the recording/playback stub with real stream graph handling.
3. Add per-application config overlays and richer routing policy.
4. Add GoogleTest and hardware-assisted validation on Ubuntu 22.04.
