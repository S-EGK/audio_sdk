#include <iostream>

#include "example_utils.h"

int main() {
    audio_sdk::AudioManager manager;

    std::cout << "Discovered devices:\n";
    audio_sdk_examples::PrintDevices(manager.EnumerateDevices());

    const bool input_selected =
        audio_sdk_examples::SelectFirstDevice(manager, audio_sdk::DeviceDirection::kInput);
    const bool output_selected =
        audio_sdk_examples::SelectFirstDevice(manager, audio_sdk::DeviceDirection::kOutput);

    return input_selected && output_selected ? 0 : 1;
}
