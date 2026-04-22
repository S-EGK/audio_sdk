# audio_sdk Examples

The examples are split by language:

- `cpp/`: C++ examples built by CMake.
- `python/`: Python examples that use the `audio_sdk` Python package.

## C++ Examples

Build all C++ examples from the repository root:

```bash
mkdir -p build
cd build
cmake -DBUILD_EXAMPLES=true ..
make
```

Run an example from `build/`:

```bash
./audio_sdk_example_enumerate_devices
./audio_sdk_example_config_file
./audio_sdk_example_select_devices
./audio_sdk_example_events
./audio_sdk_example_recording
./audio_sdk_example_playback_and_delete
./audio_sdk_example_sessions
./audio_sdk_example_fallback_policy
```

`audio_sdk_record_playback_demo` is kept as a compatibility target and builds the session-based record/playback demo.

## Python Examples

Python examples require the optional Python bindings. Install Python development headers, `pip`, and `pybind11`, then build:

```bash
mkdir -p build
cd build
cmake -DBUILD_PYTHON_BINDINGS=true ..
make
```

By default, CMake uses the active Python 3 interpreter from `PATH`. Activate a conda or venv environment before running `cmake` when the editable package should be registered into that environment. Use `-DPYTHON_EXECUTABLE="$(which python)"` only when you need to force an interpreter explicitly. If an existing `build/` directory cached an old override, clear it once with `cmake .. -DPYTHON_EXECUTABLE=`.

When `BUILD_PYTHON_BINDINGS=true`, `make` builds `_audio_sdk` and registers the editable `audio_sdk` Python package. After `make`, the package can be imported from any working directory:

```bash
python3 -c "import audio_sdk; print(audio_sdk.__version__)"
```

Run examples from the repository root with Python module mode:

```bash
python3 -m examples.python.01_enumerate_devices
python3 -m examples.python.02_config_file
python3 -m examples.python.03_select_devices
python3 -m examples.python.04_events
python3 -m examples.python.05_recording
python3 -m examples.python.06_playback_and_delete
python3 -m examples.python.07_sessions
python3 -m examples.python.08_fallback_policy
```

You can also run script files directly from `examples/python`:

```bash
cd /home/chiefs/audio_sdk/examples/python
python3 01_enumerate_devices.py
```

Common CMake flags:

- `BUILD_PYTHON_BINDINGS`: build `_audio_sdk` and register the editable `audio_sdk` Python package during `make`.
- `PYTHON_EXECUTABLE`: optional Python interpreter override. When omitted, the active Python 3 interpreter is auto-detected from `PATH`.
- `BUILD_EXAMPLES`: build C++ examples as part of `make`.
- `AUDIO_SDK_BUILD_HIL_TESTS`: include hardware-in-the-loop tests.

## Hardware Notes

Examples that record or play audio require a running PipeWire session and available input/output devices. The config and event examples can run without recording audio, but backend monitoring still depends on PipeWire.
