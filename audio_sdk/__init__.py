"""Python control-plane API for audio_sdk.

The C++ extension is built as ``_audio_sdk``. During local source builds this
package can load it from ``build/audio_sdk`` without requiring installation.
"""

from __future__ import annotations

import importlib.machinery
import importlib.util
import sys
import time
from pathlib import Path


__version__ = "0.1.0"


def _load_extension():
    package_dir = Path(__file__).resolve().parent
    repo_root = package_dir.parent
    search_dirs = [
        package_dir,
        repo_root / "build" / "audio_sdk",
        repo_root / "build",
    ]

    for directory in search_dirs:
        for suffix in importlib.machinery.EXTENSION_SUFFIXES:
            candidate = directory / f"_audio_sdk{suffix}"
            if not candidate.exists():
                continue

            spec = importlib.util.spec_from_file_location("audio_sdk._audio_sdk", candidate)
            if spec is None or spec.loader is None:
                continue

            module = importlib.util.module_from_spec(spec)
            sys.modules["audio_sdk._audio_sdk"] = module
            spec.loader.exec_module(module)
            return module

    raise ModuleNotFoundError(
        "Could not import audio_sdk._audio_sdk. Build the Python bindings first:\n"
        "  cd /home/chiefs/audio_sdk\n"
        "  mkdir -p build\n"
        "  cd build\n"
        "  cmake -DBUILD_PYTHON_BINDINGS=true ..\n"
        "  make"
    )


_extension = _load_extension()

for _name in dir(_extension):
    if not _name.startswith("_"):
        globals()[_name] = getattr(_extension, _name)


def direction_name(direction: object) -> str:
    if direction == DeviceDirection.INPUT:
        return "input"
    if direction == DeviceDirection.OUTPUT:
        return "output"
    return "unknown"


def event_type_name(event_type: object) -> str:
    names = {
        EventType.DEVICE_ADDED: "device_added",
        EventType.DEVICE_REMOVED: "device_removed",
        EventType.DEVICE_AVAILABLE_CHANGED: "device_available_changed",
        EventType.DEFAULT_INPUT_CHANGED: "default_input_changed",
        EventType.DEFAULT_OUTPUT_CHANGED: "default_output_changed",
        EventType.STREAM_STARTED: "stream_started",
        EventType.STREAM_STOPPED: "stream_stopped",
        EventType.STREAM_ERROR: "stream_error",
        EventType.STREAM_FALLBACK_APPLIED: "stream_fallback_applied",
        EventType.UNDERRUN: "underrun",
        EventType.OVERRUN: "overrun",
        EventType.ROUTE_CHANGED: "route_changed",
        EventType.RECORDING_SAVED: "recording_saved",
        EventType.RECORDING_DELETED: "recording_deleted",
    }
    return names.get(event_type, "unknown")


def require(status: object, operation: str) -> None:
    if status:
        return
    raise RuntimeError(f"{operation} failed: {status.message}")


def first_device(devices: list[object], direction: object) -> object | None:
    for device in devices:
        if device.available and device.direction == direction:
            return device
    return None


def select_first_device(manager: object, direction: object) -> object:
    device = first_device(manager.enumerate_devices(), direction)
    if device is None:
        raise RuntimeError(f"No available {direction_name(direction)} device")

    if direction == DeviceDirection.INPUT:
        require(manager.select_input_device(device.id), "select input device")
    else:
        require(manager.select_output_device(device.id), "select output device")

    return device


def recording_path(filename: str, root: str | Path = ".") -> Path:
    path = Path(root) / "recordings" / filename
    path.parent.mkdir(parents=True, exist_ok=True)
    return path


def sleep_for_recording(seconds: float = 2.0) -> None:
    time.sleep(seconds)


def print_device(device: object) -> None:
    print(
        f"{direction_name(device.direction)} "
        f"id={device.id} "
        f'name=\"{device.name}\" '
        f"default={'yes' if device.is_default else 'no'} "
        f"available={'yes' if device.available else 'no'}"
    )


def print_devices(devices: list[object]) -> None:
    if not devices:
        print("No devices reported by backend")
        return
    for device in devices:
        print_device(device)


__all__ = [
    name
    for name in globals()
    if not name.startswith("_") and name not in {"Path", "annotations", "importlib", "sys", "time"}
]
