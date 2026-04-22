import audio_sdk


def main() -> None:
    manager = audio_sdk.AudioManager()
    print(f"backend={manager.backend_name()}")
    audio_sdk.print_devices(manager.enumerate_devices())


if __name__ == "__main__":
    main()
