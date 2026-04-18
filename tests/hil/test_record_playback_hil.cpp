#include <iostream>

#include "audio_sdk/audio_manager.h"

int main() {
    std::cout << "HIL placeholder: wire a real mic and speaker, then replace this with PipeWire-backed validation.\n";
    audio_sdk::AudioManager manager;
    std::cout << "Backend: " << manager.BackendName() << '\n';
    return 0;
}

