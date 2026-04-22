#include <iostream>

#include "example_utils.h"

int main() {
    audio_sdk::AudioManager manager;

    std::cout << "backend=" << manager.BackendName() << '\n';
    audio_sdk_examples::PrintDevices(manager.EnumerateDevices());
    return 0;
}
