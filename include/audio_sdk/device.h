#pragma once

#include <string>

namespace audio_sdk {

enum class DeviceDirection {
    kInput,
    kOutput,
};

struct DeviceDescriptor {
    // Stable SDK identifier when the backend exposes stable device properties.
    // PipeWire currently uses short ids such as "in-12ab34cd56" and "out-12ab34cd56".
    std::string id;
    std::string name;
    DeviceDirection direction = DeviceDirection::kInput;
    bool is_default = false;
    bool available = true;
};

}  // namespace audio_sdk
