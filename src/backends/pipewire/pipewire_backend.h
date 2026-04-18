#pragma once

#include <functional>
#include <string>
#include <vector>

#include "audio_sdk/config.h"
#include "audio_sdk/device.h"
#include "audio_sdk/events.h"
#include "audio_sdk/status.h"

namespace audio_sdk {

struct PipeWireDeviceEvent {
    EventType type = EventType::kDeviceAdded;
    DeviceDescriptor device;
    std::vector<DeviceDescriptor> devices;
};

class PipeWireBackend {
  public:
    PipeWireBackend();
    ~PipeWireBackend();

    PipeWireBackend(const PipeWireBackend&) = delete;
    PipeWireBackend& operator=(const PipeWireBackend&) = delete;

    std::string Name() const;
    std::vector<DeviceDescriptor> EnumerateDevices() const;
    Status StartMonitoring(std::function<void(const PipeWireDeviceEvent&)> callback);
    void StopMonitoring();
    Status StartRecording(const std::string& path, const std::string& target_device_id, const StreamPolicy& policy);
    Status StopRecording();
    Status PlayRecording(const std::string& path, const std::string& target_device_id);

  private:
    class RegistrySession;
    class RecordingSession;

    std::function<void(const PipeWireDeviceEvent&)> callback_;
    RegistrySession* monitor_ = nullptr;
    RecordingSession* recorder_ = nullptr;
};

}  // namespace audio_sdk
