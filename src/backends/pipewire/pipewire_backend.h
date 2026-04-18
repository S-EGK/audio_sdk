#pragma once

#include <string>
#include <vector>

#include "audio_sdk/device.h"

namespace audio_sdk {

class PipeWireBackend {
  public:
    PipeWireBackend();
    ~PipeWireBackend();

    std::string Name() const;
    std::vector<DeviceDescriptor> EnumerateDevices() const;
};

}  // namespace audio_sdk

