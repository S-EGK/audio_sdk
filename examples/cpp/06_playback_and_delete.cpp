#include <filesystem>
#include <iostream>

#include "example_utils.h"

int main(int argc, char** argv) {
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

    const std::filesystem::path path =
        argc > 1 ? std::filesystem::path(argv[1]) : audio_sdk_examples::RecordingPath("cpp_playback_example.wav");

    if (!std::filesystem::exists(path)) {
        std::cout << "No existing WAV supplied, recording a short clip first: " << path << '\n';
        if (!audio_sdk_examples::Require(manager.StartRecording(path.string()), "start recording")) {
            return 1;
        }
        audio_sdk_examples::SleepForRecording();
        if (!audio_sdk_examples::Require(manager.StopRecording(), "stop recording")) {
            return 1;
        }
    }

    if (!audio_sdk_examples::Require(manager.PlayRecording(path.string()), "play recording")) {
        return 1;
    }

    if (!audio_sdk_examples::Require(manager.DeleteRecording(path.string()), "delete recording")) {
        return 1;
    }

    std::cout << "Played and deleted " << path << '\n';
    return 0;
}
