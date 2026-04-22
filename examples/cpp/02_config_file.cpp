#include <filesystem>
#include <iostream>

#include "example_utils.h"

int main() {
    const std::filesystem::path config_path =
        std::filesystem::current_path() / "audio_sdk_example_config.json";

    audio_sdk::AudioConfig config;
    config.input.preferred_name = "Primary Microphone";
    config.input.fallback_name = "Backup Microphone";
    config.output.preferred_name = "Primary Speaker";
    config.output.fallback_name = "Backup Speaker";
    config.recording.output_path = "recordings/config_example.wav";
    config.recording.delete_after_playback = true;
    config.stream.sample_rate = 48000;
    config.stream.channels = 2;
    config.stream.strict_sync = true;

    audio_sdk::AudioManager manager(config);
    if (!audio_sdk_examples::Require(manager.SaveConfigToFile(config_path.string()), "save config")) {
        return 1;
    }

    audio_sdk::AudioManager loaded_manager;
    if (!audio_sdk_examples::Require(loaded_manager.LoadConfigFromFile(config_path.string()), "load config")) {
        return 1;
    }

    const audio_sdk::AudioConfig& loaded = loaded_manager.config();
    std::cout << "saved_config=" << config_path << '\n';
    std::cout << "backend=" << loaded.backend << '\n';
    std::cout << "input_preferred_name=" << loaded.input.preferred_name << '\n';
    std::cout << "input_fallback_name=" << loaded.input.fallback_name << '\n';
    std::cout << "output_preferred_name=" << loaded.output.preferred_name << '\n';
    std::cout << "output_fallback_name=" << loaded.output.fallback_name << '\n';
    std::cout << "recording_output_path=" << loaded.recording.output_path << '\n';
    std::cout << "stream_sample_rate=" << loaded.stream.sample_rate << '\n';
    std::cout << "stream_channels=" << loaded.stream.channels << '\n';
    std::cout << "stream_strict_sync=" << (loaded.stream.strict_sync ? "true" : "false") << '\n';
    return 0;
}
