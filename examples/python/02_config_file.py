from pathlib import Path

import audio_sdk


def main() -> None:
    config_path = Path.cwd() / "build" / "audio_sdk_example_config.json"
    config_path.parent.mkdir(parents=True, exist_ok=True)

    config = audio_sdk.AudioConfig()
    config.input.preferred_name = "Primary Microphone"
    config.input.fallback_name = "Backup Microphone"
    config.output.preferred_name = "Primary Speaker"
    config.output.fallback_name = "Backup Speaker"
    config.recording.output_path = "recordings/config_example.wav"
    config.recording.delete_after_playback = True
    config.stream.sample_rate = 48000
    config.stream.channels = 2
    config.stream.strict_sync = True

    manager = audio_sdk.AudioManager(config)
    audio_sdk.require(manager.save_config_to_file(str(config_path)), "save config")

    loaded_manager = audio_sdk.AudioManager()
    audio_sdk.require(loaded_manager.load_config_from_file(str(config_path)), "load config")
    loaded = loaded_manager.config

    print(f"saved_config={config_path}")
    print(f"backend={loaded.backend}")
    print(f"input_preferred_name={loaded.input.preferred_name}")
    print(f"input_fallback_name={loaded.input.fallback_name}")
    print(f"output_preferred_name={loaded.output.preferred_name}")
    print(f"output_fallback_name={loaded.output.fallback_name}")
    print(f"recording_output_path={loaded.recording.output_path}")
    print(f"stream_sample_rate={loaded.stream.sample_rate}")
    print(f"stream_channels={loaded.stream.channels}")
    print(f"stream_strict_sync={loaded.stream.strict_sync}")


if __name__ == "__main__":
    main()
