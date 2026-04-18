#include "pipewire_backend.h"

#include <pipewire/pipewire.h>

namespace audio_sdk {

PipeWireBackend::PipeWireBackend() {
    pw_init(nullptr, nullptr);
}

PipeWireBackend::~PipeWireBackend() {
    pw_deinit();
}

std::string PipeWireBackend::Name() const {
    return "pipewire";
}

std::vector<DeviceDescriptor> PipeWireBackend::EnumerateDevices() const {
    // Device enumeration will be implemented with the PipeWire registry.
    return {};
}

}  // namespace audio_sdk
