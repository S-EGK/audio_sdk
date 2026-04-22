#include <iostream>

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

    const std::filesystem::path path = audio_sdk_examples::RecordingPath("cpp_recording_example.wav");
    if (!audio_sdk_examples::Require(manager.StartRecording(path.string()), "start recording")) {
        return 1;
    }

    std::cout << "Recording for two seconds to " << path << '\n';
    audio_sdk_examples::SleepForRecording();

    if (!audio_sdk_examples::Require(manager.StopRecording(), "stop recording")) {
        return 1;
    }

    std::cout << "Saved " << path << '\n';
    return 0;
}
