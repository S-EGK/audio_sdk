#pragma once

#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "audio_sdk/audio_manager.h"

namespace audio_sdk_examples {

inline std::string DirectionName(audio_sdk::DeviceDirection direction) {
    return direction == audio_sdk::DeviceDirection::kInput ? "input" : "output";
}

inline std::string EventTypeName(audio_sdk::EventType type) {
    switch (type) {
        case audio_sdk::EventType::kDeviceAdded:
            return "device_added";
        case audio_sdk::EventType::kDeviceRemoved:
            return "device_removed";
        case audio_sdk::EventType::kDeviceAvailableChanged:
            return "device_available_changed";
        case audio_sdk::EventType::kDefaultInputChanged:
            return "default_input_changed";
        case audio_sdk::EventType::kDefaultOutputChanged:
            return "default_output_changed";
        case audio_sdk::EventType::kStreamStarted:
            return "stream_started";
        case audio_sdk::EventType::kStreamStopped:
            return "stream_stopped";
        case audio_sdk::EventType::kStreamError:
            return "stream_error";
        case audio_sdk::EventType::kStreamFallbackApplied:
            return "stream_fallback_applied";
        case audio_sdk::EventType::kUnderrun:
            return "underrun";
        case audio_sdk::EventType::kOverrun:
            return "overrun";
        case audio_sdk::EventType::kRouteChanged:
            return "route_changed";
        case audio_sdk::EventType::kRecordingSaved:
            return "recording_saved";
        case audio_sdk::EventType::kRecordingDeleted:
            return "recording_deleted";
    }
    return "unknown";
}

inline bool Require(const audio_sdk::Status& status, const std::string& operation) {
    if (status) {
        return true;
    }

    std::cerr << operation << " failed: " << status.message << '\n';
    return false;
}

inline std::filesystem::path RecordingPath(const std::string& filename) {
    std::filesystem::path path = std::filesystem::current_path() / "recordings" / filename;
    std::filesystem::create_directories(path.parent_path());
    return path;
}

inline std::optional<audio_sdk::DeviceDescriptor> FirstDevice(
    const std::vector<audio_sdk::DeviceDescriptor>& devices,
    audio_sdk::DeviceDirection direction) {
    for (const auto& device : devices) {
        if (device.available && device.direction == direction) {
            return device;
        }
    }
    return std::nullopt;
}

inline void PrintDevice(const audio_sdk::DeviceDescriptor& device) {
    std::cout << DirectionName(device.direction)
              << " id=" << device.id
              << " name=\"" << device.name << "\""
              << " default=" << (device.is_default ? "yes" : "no")
              << " available=" << (device.available ? "yes" : "no")
              << '\n';
}

inline void PrintDevices(const std::vector<audio_sdk::DeviceDescriptor>& devices) {
    if (devices.empty()) {
        std::cout << "No devices reported by backend\n";
        return;
    }

    for (const auto& device : devices) {
        PrintDevice(device);
    }
}

inline bool SelectFirstDevice(audio_sdk::AudioManager& manager, audio_sdk::DeviceDirection direction) {
    const auto devices = manager.EnumerateDevices();
    const auto device = FirstDevice(devices, direction);
    if (!device) {
        std::cerr << "No available " << DirectionName(direction) << " device\n";
        return false;
    }

    const audio_sdk::Status status =
        direction == audio_sdk::DeviceDirection::kInput
            ? manager.SelectInputDevice(device->id)
            : manager.SelectOutputDevice(device->id);
    if (!Require(status, "select " + DirectionName(direction) + " device")) {
        return false;
    }

    std::cout << "Selected " << DirectionName(direction) << " device: "
              << device->name << " (" << device->id << ")\n";
    return true;
}

inline void SleepForRecording() {
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

}  // namespace audio_sdk_examples
