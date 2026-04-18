#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "audio_sdk/config.h"
#include "audio_sdk/device.h"
#include "audio_sdk/events.h"
#include "audio_sdk/status.h"

namespace audio_sdk {

class PipeWireBackend;

class AudioManager {
  public:
    explicit AudioManager(AudioConfig config = {});
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    Status LoadConfigFromFile(const std::string& path);
    Status SaveConfigToFile(const std::string& path) const;

    std::string BackendName() const;
    std::vector<DeviceDescriptor> EnumerateDevices() const;

    Status SelectInputDevice(const std::string& device_id);
    Status SelectOutputDevice(const std::string& device_id);

    Status StartRecording(const std::string& path);
    Status StopRecording();
    Status PlayRecording(const std::string& path);
    Status DeleteRecording(const std::string& path);

    void SetEventCallback(EventCallback callback);

    const AudioConfig& config() const { return config_; }

  private:
    void Emit(EventType type, std::string device_id, std::string details) const;

    AudioConfig config_;
    bool recording_active_ = false;
    std::string active_recording_path_;
    EventCallback callback_;
    std::unique_ptr<PipeWireBackend> backend_;
    mutable std::mutex callback_mutex_;
};

}  // namespace audio_sdk
