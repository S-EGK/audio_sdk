#include "audio_sdk/audio_manager.h"

#include <filesystem>
#include <utility>

#include "backends/pipewire/pipewire_backend.h"

namespace audio_sdk {

AudioManager::AudioManager(AudioConfig config)
    : config_(std::move(config)),
      backend_(std::make_unique<PipeWireBackend>()) {}

AudioManager::~AudioManager() {
    if (backend_) {
        backend_->StopMonitoring();
    }
}

Status AudioManager::LoadConfigFromFile(const std::string& path) {
    return LoadConfig(path, config_);
}

Status AudioManager::SaveConfigToFile(const std::string& path) const {
    return SaveConfig(path, config_);
}

std::string AudioManager::BackendName() const {
    return backend_->Name();
}

std::vector<DeviceDescriptor> AudioManager::EnumerateDevices() const {
    return backend_->EnumerateDevices();
}

Status AudioManager::SelectInputDevice(const std::string& device_id) {
    config_.input.preferred_id = device_id;
    Emit(EventType::kRouteChanged, device_id, "Selected input device");
    return Status::Ok();
}

Status AudioManager::SelectOutputDevice(const std::string& device_id) {
    config_.output.preferred_id = device_id;
    Emit(EventType::kRouteChanged, device_id, "Selected output device");
    return Status::Ok();
}

Status AudioManager::StartRecording(const std::string& path) {
    if (recording_active_) {
        return Status::Error("Recording is already active");
    }

    Status status = backend_->StartRecording(path, config_.input.preferred_id, config_.stream);
    if (!status) {
        Emit(EventType::kStreamError, config_.input.preferred_id, status.message);
        return status;
    }

    recording_active_ = true;
    active_recording_path_ = path;
    Emit(EventType::kStreamStarted, config_.input.preferred_id, "Recording started");
    return Status::Ok();
}

Status AudioManager::StopRecording() {
    if (!recording_active_) {
        return Status::Error("Recording is not active");
    }

    const Status status = backend_->StopRecording();
    if (!status) {
        Emit(EventType::kStreamError, config_.input.preferred_id, status.message);
        return status;
    }

    recording_active_ = false;
    Emit(EventType::kRecordingSaved, config_.input.preferred_id, active_recording_path_);
    Emit(EventType::kStreamStopped, config_.input.preferred_id, "Recording stopped");
    return Status::Ok();
}

Status AudioManager::PlayRecording(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        return Status::Error("Recording file does not exist: " + path);
    }

    Emit(EventType::kStreamStarted, config_.output.preferred_id, "Playback started");
    const Status status = backend_->PlayRecording(path, config_.output.preferred_id);
    if (!status) {
        Emit(EventType::kStreamError, config_.output.preferred_id, status.message);
        return status;
    }

    Emit(EventType::kStreamStopped, config_.output.preferred_id, "Playback stopped");
    return Status::Ok();
}

Status AudioManager::DeleteRecording(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        return Status::Error("Recording file does not exist: " + path);
    }

    std::filesystem::remove(path);
    Emit(EventType::kRecordingDeleted, {}, path);
    return Status::Ok();
}

void AudioManager::SetEventCallback(EventCallback callback) {
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback_ = std::move(callback);
    }

    if (!backend_) {
        return;
    }

    if (!callback_) {
        backend_->StopMonitoring();
        return;
    }

    const Status status = backend_->StartMonitoring([this](const PipeWireDeviceEvent& event) {
        const std::string details = event.device.name.empty() ? event.device.id : event.device.name;
        Emit(event.type, event.device.id, details);
    });

    if (!status) {
        Emit(EventType::kStreamError, {}, status.message);
    }
}

void AudioManager::Emit(EventType type, std::string device_id, std::string details) const {
    EventCallback callback_copy;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback_copy = callback_;
    }

    if (!callback_copy) {
        return;
    }

    callback_copy(Event{type, std::move(device_id), std::move(details)});
}

}  // namespace audio_sdk
