#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

#include <unistd.h>

#include "audio_sdk/audio_manager.h"

int main() {
    namespace fs = std::filesystem;

    std::cout << "Running PipeWire HIL record/playback validation\n";
    audio_sdk::AudioManager discovery_manager;

    const auto devices = discovery_manager.EnumerateDevices();

    std::string input_id;
    std::string input_name;
    std::string output_id;
    std::string output_name;
    for (const auto& device : devices) {
        if (device.direction == audio_sdk::DeviceDirection::kInput && input_id.empty()) {
            input_id = device.id;
            input_name = device.name;
        }
        if (device.direction == audio_sdk::DeviceDirection::kOutput && output_id.empty()) {
            output_id = device.id;
            output_name = device.name;
        }
    }

    if (input_id.empty() || output_id.empty()) {
        std::cerr << "Expected at least one input and one output device\n";
        return 1;
    }

    audio_sdk::AudioConfig config;
    config.input.preferred_name = input_name;
    config.output.preferred_name = output_name;
    audio_sdk::AudioManager manager(config);

    const fs::path temp_root = fs::temp_directory_path() / ("audio_sdk_hil_" + std::to_string(getpid()));
    const fs::path wav_path = temp_root / "hil.wav";
    fs::create_directories(temp_root);

    manager.SetEventCallback([](const audio_sdk::Event& event) {
        std::cout << "event=" << static_cast<int>(event.type)
                  << " device=" << event.device_id
                  << " details=" << event.details << '\n';
    });

    assert(manager.StartRecording(wav_path.string()));
    std::this_thread::sleep_for(std::chrono::seconds(2));
    assert(manager.StopRecording());
    assert(fs::exists(wav_path));
    assert(fs::file_size(wav_path) > 44);

    assert(manager.PlayRecording(wav_path.string()));
    assert(manager.DeleteRecording(wav_path.string()));
    assert(!fs::exists(wav_path));

    fs::remove_all(temp_root);
    return 0;
}
