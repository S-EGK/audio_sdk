import audio_sdk


def main() -> None:
    manager = audio_sdk.AudioManager()

    print("Discovered devices:")
    audio_sdk.print_devices(manager.enumerate_devices())

    input_device = audio_sdk.select_first_device(manager, audio_sdk.DeviceDirection.INPUT)
    output_device = audio_sdk.select_first_device(manager, audio_sdk.DeviceDirection.OUTPUT)
    print(f"Selected input device: {input_device.name} ({input_device.id})")
    print(f"Selected output device: {output_device.name} ({output_device.id})")


if __name__ == "__main__":
    main()
