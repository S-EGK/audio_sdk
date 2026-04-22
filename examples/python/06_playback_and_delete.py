import sys
from pathlib import Path

import audio_sdk


def main() -> None:
    manager = audio_sdk.AudioManager()

    def on_event(event: object) -> None:
        print(
            f"event={audio_sdk.event_type_name(event.type)} "
            f"device={event.device_id} "
            f'details="{event.details}"'
        )

    manager.set_event_callback(on_event)
    input_device = audio_sdk.select_first_device(manager, audio_sdk.DeviceDirection.INPUT)
    output_device = audio_sdk.select_first_device(manager, audio_sdk.DeviceDirection.OUTPUT)
    print(f"Selected input device: {input_device.name} ({input_device.id})")
    print(f"Selected output device: {output_device.name} ({output_device.id})")

    path = Path(sys.argv[1]) if len(sys.argv) > 1 else audio_sdk.recording_path("python_playback_example.wav")
    if not path.exists():
        print(f"No existing WAV supplied, recording a short clip first: {path}")
        audio_sdk.require(manager.start_recording(str(path)), "start recording")
        audio_sdk.sleep_for_recording()
        audio_sdk.require(manager.stop_recording(), "stop recording")

    audio_sdk.require(manager.play_recording(str(path)), "play recording")
    audio_sdk.require(manager.delete_recording(str(path)), "delete recording")
    print(f"Played and deleted {path}")


if __name__ == "__main__":
    main()
