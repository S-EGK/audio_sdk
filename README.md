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

The current implementation now covers real PipeWire-backed device discovery, event monitoring, WAV recording, playback, and deletion. Fallback policy, richer routing, and stricter synchronization controls still need to be built out.

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
mkdir build
cd build
cmake ..
make
ctest --output-on-failure
```

Python bindings are built only when both `Python3` and `pybind11` are installed.

## Git Workflow

- Keep `main` as the stable integration branch.
- Do feature work on `feature/<topic>` branches such as `feature/live-stream-sessions`.
- Use short imperative commit subjects such as `Add active stream fallback policy`.
- Tag releases from `main` with annotated semantic versions such as `v0.1.0` or `v0.1.1`.

Typical flow:

```bash
git checkout main
git pull
git checkout -b feature/live-stream-sessions
# work, test, commit
git push -u origin feature/live-stream-sessions
```

Release flow:

```bash
git checkout main
git pull
git tag -a v0.1.0 -m "Release v0.1.0"
git push origin v0.1.0
```

## Recommended Direction

- Use PipeWire for real-time graph and strict-sync stream work.
- Keep Python out of real-time callbacks.
- Store checked-in app defaults in the consuming app repo and write runtime overrides to an external state file.
- Add deeper backend features incrementally behind the same public API.

## Near-Term Roadmap

1. Add config-driven device fallback and hot-plug recovery behavior.
2. Expand routing and live-stream session control beyond record/playback.
3. Replace the temporary JSON parser with a proper JSON dependency.
4. Add deeper hardware-assisted validation on Ubuntu 22.04.
