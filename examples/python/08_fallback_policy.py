import audio_sdk


def main() -> None:
    discovery_manager = audio_sdk.AudioManager()
    devices = discovery_manager.enumerate_devices()
    fallback_input = audio_sdk.first_device(devices, audio_sdk.DeviceDirection.INPUT)
    if fallback_input is None:
        raise SystemExit("No available input device for fallback policy example")

    config = audio_sdk.AudioConfig()
    config.input.preferred_id = "missing-input-device-id"
    config.input.fallback_id = fallback_input.id

    manager = audio_sdk.AudioManager(config)

    def on_event(event: object) -> None:
        print(
            f"event={audio_sdk.event_type_name(event.type)} "
            f"device={event.device_id} "
            f'details="{event.details}"'
        )

    manager.set_event_callback(on_event)

    path = audio_sdk.recording_path("python_fallback_policy_example.wav")
    print(
        "Preferred input is intentionally missing; fallback should be "
        f"{fallback_input.name} ({fallback_input.id})"
    )

    audio_sdk.require(manager.start_recording(str(path)), "start fallback recording")
    audio_sdk.sleep_for_recording()
    audio_sdk.require(manager.stop_recording(), "stop fallback recording")
    print(f"Fallback recording completed at {path}")


if __name__ == "__main__":
    main()
