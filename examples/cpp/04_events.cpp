#include <iostream>

#include "example_utils.h"

int main() {
    audio_sdk::AudioManager manager;

    manager.SetEventCallback([](const audio_sdk::Event& event) {
        std::cout << "event=" << audio_sdk_examples::EventTypeName(event.type)
                  << " device=" << event.device_id
                  << " details=\"" << event.details << "\"\n";
    });

    std::cout << "Selecting example routes to trigger route-change events\n";
    if (!audio_sdk_examples::Require(manager.SelectInputDevice("example-input-id"), "select input device")) {
        return 1;
    }
    if (!audio_sdk_examples::Require(manager.SelectOutputDevice("example-output-id"), "select output device")) {
        return 1;
    }

    std::cout << "Watch this process while adding/removing PipeWire devices to see device events\n";
    return 0;
}
