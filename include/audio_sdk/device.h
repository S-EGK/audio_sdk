#pragma once

#include <string>

namespace audio_sdk {

enum class DeviceDirection {
    kInput,
    kOutput,
};

struct DeviceDescriptor {
    std::string id;
    std::string name;
    DeviceDirection direction = DeviceDirection::kInput;
    bool is_default = false;
    bool available = true;
};

}  // namespace audio_sdk

