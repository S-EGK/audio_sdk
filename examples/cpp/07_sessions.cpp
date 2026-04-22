#include <chrono>
#include <iostream>
#include <thread>

#include "example_utils.h"

int main() {
    audio_sdk::AudioManager manager;

    manager.SetEventCallback([](const audio_sdk::Event& event) {
        std::cout << "event=" << audio_sdk_examples::EventTypeName(event.type)
                  << " device=" << event.device_id
                  << " details=\"" << event.details << "\"\n";
    });

    if (!audio_sdk_examples::SelectFirstDevice(manager, audio_sdk::DeviceDirection::kInput)) {
        return 1;
    }
    if (!audio_sdk_examples::SelectFirstDevice(manager, audio_sdk::DeviceDirection::kOutput)) {
        return 1;
    }

    const std::filesystem::path path = audio_sdk_examples::RecordingPath("cpp_session_example.wav");

    auto recording_session = manager.CreateRecordingSession(path.string());
    if (!recording_session || !audio_sdk_examples::Require(recording_session->Start(), "start recording session")) {
        return 1;
    }

    std::cout << "Recording session active=" << (recording_session->active() ? "yes" : "no") << '\n';
    audio_sdk_examples::SleepForRecording();

    if (!audio_sdk_examples::Require(recording_session->Stop(), "stop recording session")) {
        return 1;
    }

    auto playback_session = manager.CreatePlaybackSession(path.string());
    if (!playback_session || !audio_sdk_examples::Require(playback_session->Start(), "start playback session")) {
        return 1;
    }

    while (playback_session->active()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Session-based record/playback completed at " << path << '\n';
    return 0;
}
