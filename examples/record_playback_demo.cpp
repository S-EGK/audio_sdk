#include <chrono>
#include <iostream>
#include <thread>

#include "audio_sdk/audio_manager.h"

int main() {
    audio_sdk::AudioManager manager;

    manager.SetEventCallback([](const audio_sdk::Event& event) {
        std::cout << "event=" << static_cast<int>(event.type)
                  << " device=" << event.device_id
                  << " details=" << event.details << '\n';
    });

    const std::string path = "recordings/demo.wav";

    auto recording_session = manager.CreateRecordingSession(path);
    if (!recording_session || !recording_session->Start()) {
        std::cerr << "Failed to start recording\n";
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (!recording_session->Stop()) {
        std::cerr << "Failed to stop recording\n";
        return 1;
    }

    auto playback_session = manager.CreatePlaybackSession(path);
    if (!playback_session || !playback_session->Start()) {
        std::cerr << "Failed to play recording\n";
        return 1;
    }

    while (playback_session->active()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Demo completed with session-based record/playback at " << path << '\n';
    return 0;
}
