#include <iostream>

#include "audio_sdk/audio_manager.h"

int main() {
    audio_sdk::AudioManager manager;

    manager.SetEventCallback([](const audio_sdk::Event& event) {
        std::cout << "event=" << static_cast<int>(event.type)
                  << " device=" << event.device_id
                  << " details=" << event.details << '\n';
    });

    const std::string path = "recordings/demo.wav";

    if (!manager.StartRecording(path)) {
        std::cerr << "Failed to start recording\n";
        return 1;
    }

    if (!manager.StopRecording()) {
        std::cerr << "Failed to stop recording\n";
        return 1;
    }

    if (!manager.PlayRecording(path)) {
        std::cerr << "Failed to play recording\n";
        return 1;
    }

    std::cout << "Demo completed with placeholder WAV output at " << path << '\n';
    return 0;
}

