#include <iostream>

#include "example_utils.h"

int main() {
    audio_sdk::AudioManager discovery_manager;
    const auto devices = discovery_manager.EnumerateDevices();
    const auto fallback_input = audio_sdk_examples::FirstDevice(devices, audio_sdk::DeviceDirection::kInput);

    if (!fallback_input) {
        std::cerr << "No available input device for fallback policy example\n";
        return 1;
    }

    audio_sdk::AudioConfig config;
    config.input.preferred_id = "missing-input-device-id";
    config.input.fallback_id = fallback_input->id;

    audio_sdk::AudioManager manager(config);
    manager.SetEventCallback([](const audio_sdk::Event& event) {
        std::cout << "event=" << audio_sdk_examples::EventTypeName(event.type)
                  << " device=" << event.device_id
                  << " details=\"" << event.details << "\"\n";
    });

    const std::filesystem::path path = audio_sdk_examples::RecordingPath("cpp_fallback_policy_example.wav");
    std::cout << "Preferred input is intentionally missing; fallback should be "
              << fallback_input->name << " (" << fallback_input->id << ")\n";

    if (!audio_sdk_examples::Require(manager.StartRecording(path.string()), "start fallback recording")) {
        return 1;
    }

    audio_sdk_examples::SleepForRecording();
    if (!audio_sdk_examples::Require(manager.StopRecording(), "stop fallback recording")) {
        return 1;
    }

    std::cout << "Fallback recording completed at " << path << '\n';
    return 0;
}
