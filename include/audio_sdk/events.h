#pragma once

#include <functional>
#include <string>

namespace audio_sdk {

enum class EventType {
    kDeviceAdded,
    kDeviceRemoved,
    kDeviceAvailableChanged,
    kDefaultInputChanged,
    kDefaultOutputChanged,
    kStreamStarted,
    kStreamStopped,
    kStreamError,
    kStreamFallbackApplied,
    kUnderrun,
    kOverrun,
    kRouteChanged,
    kRecordingSaved,
    kRecordingDeleted,
};

struct Event {
    EventType type = EventType::kStreamStopped;
    std::string device_id;
    std::string details;
};

using EventCallback = std::function<void(const Event&)>;

}  // namespace audio_sdk

