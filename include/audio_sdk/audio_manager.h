#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "audio_sdk/config.h"
#include "audio_sdk/device.h"
#include "audio_sdk/events.h"
#include "audio_sdk/audio_session.h"
#include "audio_sdk/status.h"

namespace audio_sdk {

class PipeWireBackend;
struct PipeWireDeviceEvent;

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
    std::unique_ptr<AudioSession> CreateRecordingSession(const std::string& path);
    std::unique_ptr<AudioSession> CreatePlaybackSession(const std::string& path);

    void SetEventCallback(EventCallback callback);

    const AudioConfig& config() const { return config_; }

  private:
    Status EnsureMonitoring();
    Status ResolveTargetDevice(DeviceDirection direction, std::string& device_id, std::string& device_name, bool& used_fallback);
    Status StartPlaybackSession(const std::string& path);
    Status StopPlaybackSession(bool emit_event = true);
    void JoinPlaybackWatcher();
    bool IsRecordingSessionActive(const std::string& path) const;
    bool IsPlaybackSessionActive(const std::string& path) const;
    void HandleBackendDeviceEvent(const PipeWireDeviceEvent& event);
    void HandleDeviceRemoval(DeviceDirection direction, const PipeWireDeviceEvent& event);
    void Emit(EventType type, std::string device_id, std::string details) const;

    AudioConfig config_;
    bool recording_active_ = false;
    bool playback_active_ = false;
    std::string active_recording_path_;
    std::string active_recording_device_id_;
    std::string active_playback_path_;
    std::string active_playback_device_id_;
    std::string selected_input_device_id_;
    std::string selected_output_device_id_;
    EventCallback callback_;
    std::unique_ptr<PipeWireBackend> backend_;
    std::thread playback_watcher_;
    std::shared_ptr<std::atomic_bool> alive_;
    bool monitoring_started_ = false;
    mutable std::mutex state_mutex_;
    mutable std::mutex callback_mutex_;
};

}  // namespace audio_sdk
