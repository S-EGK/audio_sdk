import time

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

    path = audio_sdk.recording_path("python_session_example.wav")

    recording_session = manager.create_recording_session(str(path))
    audio_sdk.require(recording_session.start(), "start recording session")
    print(f"Recording session active={recording_session.active()}")
    audio_sdk.sleep_for_recording()
    audio_sdk.require(recording_session.stop(), "stop recording session")

    playback_session = manager.create_playback_session(str(path))
    audio_sdk.require(playback_session.start(), "start playback session")
    while playback_session.active():
        time.sleep(0.1)

    print(f"Session-based record/playback completed at {path}")


if __name__ == "__main__":
    main()
