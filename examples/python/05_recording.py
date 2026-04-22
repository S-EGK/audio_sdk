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
    print(f"Selected input device: {input_device.name} ({input_device.id})")

    path = audio_sdk.recording_path("python_recording_example.wav")
    audio_sdk.require(manager.start_recording(str(path)), "start recording")

    print(f"Recording for two seconds to {path}")
    audio_sdk.sleep_for_recording()

    audio_sdk.require(manager.stop_recording(), "stop recording")
    print(f"Saved {path}")


if __name__ == "__main__":
    main()
