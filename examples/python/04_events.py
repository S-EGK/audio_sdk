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

    print("Selecting example routes to trigger route-change events")
    audio_sdk.require(manager.select_input_device("example-input-id"), "select input device")
    audio_sdk.require(manager.select_output_device("example-output-id"), "select output device")
    print("Watch this process while adding/removing PipeWire devices to see device events")


if __name__ == "__main__":
    main()
