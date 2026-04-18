#include "audio_sdk/audio_manager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <utility>

#include "backends/pipewire/pipewire_backend.h"

namespace audio_sdk {

namespace {

struct ResolvedDeviceChoice {
    bool found = false;
    bool used_fallback = false;
    DeviceDescriptor device;
};

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool NameEquals(const std::string& left, const std::string& right) {
    return !left.empty() && !right.empty() && ToLower(left) == ToLower(right);
}

bool MatchesDirection(const DeviceDescriptor& device, DeviceDirection direction) {
    return device.available && device.direction == direction;
}

ResolvedDeviceChoice FindById(
    const std::vector<DeviceDescriptor>& devices,
    DeviceDirection direction,
    const std::string& device_id,
    bool used_fallback) {
    if (device_id.empty()) {
        return {};
    }

    for (const auto& device : devices) {
        if (MatchesDirection(device, direction) && device.id == device_id) {
            return ResolvedDeviceChoice{true, used_fallback, device};
        }
    }
    return {};
}

ResolvedDeviceChoice FindByName(
    const std::vector<DeviceDescriptor>& devices,
    DeviceDirection direction,
    const std::string& device_name,
    bool used_fallback) {
    if (device_name.empty()) {
        return {};
    }

    for (const auto& device : devices) {
        if (MatchesDirection(device, direction) && NameEquals(device.name, device_name)) {
            return ResolvedDeviceChoice{true, used_fallback, device};
        }
    }
    return {};
}

ResolvedDeviceChoice ResolveDeviceChoice(
    const std::vector<DeviceDescriptor>& devices,
    DeviceDirection direction,
    const DevicePolicy& policy) {
    ResolvedDeviceChoice choice = FindById(devices, direction, policy.preferred_id, false);
    if (choice.found) {
        return choice;
    }

    choice = FindByName(devices, direction, policy.preferred_name, false);
    if (choice.found) {
        return choice;
    }

    choice = FindById(devices, direction, policy.fallback_id, true);
    if (choice.found) {
        return choice;
    }

    choice = FindByName(devices, direction, policy.fallback_name, true);
    if (choice.found) {
        return choice;
    }

    const bool policy_configured =
        !policy.preferred_id.empty() ||
        !policy.preferred_name.empty() ||
        !policy.fallback_id.empty() ||
        !policy.fallback_name.empty();
    if (policy_configured) {
        return {};
    }

    for (const auto& device : devices) {
        if (MatchesDirection(device, direction)) {
            return ResolvedDeviceChoice{true, false, device};
        }
    }
    return {};
}

std::string DirectionLabel(DeviceDirection direction) {
    return direction == DeviceDirection::kInput ? "input" : "output";
}

}  // namespace

AudioManager::AudioManager(AudioConfig config)
    : config_(std::move(config)),
      backend_(std::make_unique<PipeWireBackend>()),
      alive_(std::make_shared<std::atomic_bool>(true)) {
    static_cast<void>(EnsureMonitoring());
}

AudioManager::~AudioManager() {
    if (alive_) {
        alive_->store(false);
    }
    static_cast<void>(StopPlaybackSession(false));
    JoinPlaybackWatcher();
    if (backend_) {
        backend_->StopMonitoring();
    }
}

Status AudioManager::LoadConfigFromFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    const Status status = LoadConfig(path, config_);
    if (!status) {
        return status;
    }

    selected_input_device_id_.clear();
    selected_output_device_id_.clear();
    return Status::Ok();
}

Status AudioManager::SaveConfigToFile(const std::string& path) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return SaveConfig(path, config_);
}

std::string AudioManager::BackendName() const {
    return backend_->Name();
}

std::vector<DeviceDescriptor> AudioManager::EnumerateDevices() const {
    return backend_->EnumerateDevices();
}

Status AudioManager::SelectInputDevice(const std::string& device_id) {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        config_.input.preferred_id = device_id;
        config_.input.preferred_name.clear();
        selected_input_device_id_.clear();
    }
    Emit(EventType::kRouteChanged, device_id, "Selected input device");
    return Status::Ok();
}

Status AudioManager::SelectOutputDevice(const std::string& device_id) {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        config_.output.preferred_id = device_id;
        config_.output.preferred_name.clear();
        selected_output_device_id_.clear();
    }
    Emit(EventType::kRouteChanged, device_id, "Selected output device");
    return Status::Ok();
}

Status AudioManager::StartRecording(const std::string& path) {
    StreamPolicy stream_policy;
    std::string target_device_id;
    std::string target_device_name;
    bool used_fallback = false;

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (recording_active_) {
            return Status::Error("Recording is already active");
        }
        stream_policy = config_.stream;
    }

    static_cast<void>(EnsureMonitoring());

    Status status = ResolveTargetDevice(DeviceDirection::kInput, target_device_id, target_device_name, used_fallback);
    if (!status) {
        Emit(EventType::kStreamError, {}, status.message);
        return status;
    }

    if (used_fallback) {
        Emit(
            EventType::kStreamFallbackApplied,
            target_device_id,
            "Recording fell back to " + target_device_name);
    }

    status = backend_->StartRecording(path, target_device_id, stream_policy);
    if (!status) {
        Emit(EventType::kStreamError, target_device_id, status.message);
        return status;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        recording_active_ = true;
        active_recording_path_ = path;
        active_recording_device_id_ = target_device_id;
    }

    Emit(EventType::kStreamStarted, target_device_id, "Recording started on " + target_device_name);
    return Status::Ok();
}

Status AudioManager::StopRecording() {
    std::string recording_path;
    std::string recording_device_id;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (!recording_active_) {
            return Status::Error("Recording is not active");
        }
        recording_path = active_recording_path_;
        recording_device_id = active_recording_device_id_;
    }

    const Status status = backend_->StopRecording();
    if (!status) {
        Emit(EventType::kStreamError, recording_device_id, status.message);
        return status;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        recording_active_ = false;
        active_recording_path_.clear();
        active_recording_device_id_.clear();
    }

    Emit(EventType::kRecordingSaved, recording_device_id, recording_path);
    Emit(EventType::kStreamStopped, recording_device_id, "Recording stopped");
    return Status::Ok();
}

Status AudioManager::PlayRecording(const std::string& path) {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (playback_active_) {
            return Status::Error("Playback is already active");
        }
    }
    JoinPlaybackWatcher();

    std::string target_device_id;
    std::string target_device_name;
    bool used_fallback = false;

    if (!std::filesystem::exists(path)) {
        return Status::Error("Recording file does not exist: " + path);
    }

    static_cast<void>(EnsureMonitoring());

    Status status = ResolveTargetDevice(DeviceDirection::kOutput, target_device_id, target_device_name, used_fallback);
    if (!status) {
        Emit(EventType::kStreamError, {}, status.message);
        return status;
    }

    if (used_fallback) {
        Emit(
            EventType::kStreamFallbackApplied,
            target_device_id,
            "Playback fell back to " + target_device_name);
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (playback_active_) {
            return Status::Error("Playback is already active");
        }
        playback_active_ = true;
        active_playback_path_ = path;
        active_playback_device_id_ = target_device_id;
    }

    Emit(EventType::kStreamStarted, target_device_id, "Playback started on " + target_device_name);
    status = backend_->StartPlayback(path, target_device_id);
    if (!status) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            playback_active_ = false;
            active_playback_path_.clear();
            active_playback_device_id_.clear();
        }
        Emit(EventType::kStreamError, target_device_id, status.message);
        return status;
    }

    status = backend_->WaitForPlaybackToFinish();
    const Status stop_status = backend_->StopPlayback();

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        playback_active_ = false;
        active_playback_path_.clear();
        active_playback_device_id_.clear();
    }

    if (!status) {
        Emit(EventType::kStreamError, target_device_id, status.message);
        return status;
    }
    if (!stop_status) {
        Emit(EventType::kStreamError, target_device_id, stop_status.message);
        return stop_status;
    }

    Emit(EventType::kStreamStopped, target_device_id, "Playback stopped");
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

std::unique_ptr<AudioSession> AudioManager::CreateRecordingSession(const std::string& path) {
    const auto alive = alive_;
    return std::unique_ptr<AudioSession>(new AudioSession(
        AudioSessionMode::kRecording,
        path,
        [this, alive, path]() {
            if (!alive || !alive->load()) {
                return Status::Error("AudioManager is no longer available");
            }
            return StartRecording(path);
        },
        [this, alive]() {
            if (!alive || !alive->load()) {
                return Status::Error("AudioManager is no longer available");
            }
            return StopRecording();
        },
        [this, alive, path]() {
            if (!alive || !alive->load()) {
                return false;
            }
            return IsRecordingSessionActive(path);
        }));
}

std::unique_ptr<AudioSession> AudioManager::CreatePlaybackSession(const std::string& path) {
    const auto alive = alive_;
    return std::unique_ptr<AudioSession>(new AudioSession(
        AudioSessionMode::kPlayback,
        path,
        [this, alive, path]() {
            if (!alive || !alive->load()) {
                return Status::Error("AudioManager is no longer available");
            }
            return StartPlaybackSession(path);
        },
        [this, alive]() {
            if (!alive || !alive->load()) {
                return Status::Error("AudioManager is no longer available");
            }
            return StopPlaybackSession();
        },
        [this, alive, path]() {
            if (!alive || !alive->load()) {
                return false;
            }
            return IsPlaybackSessionActive(path);
        }));
}

void AudioManager::SetEventCallback(EventCallback callback) {
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback_ = std::move(callback);
    }

    const Status status = EnsureMonitoring();
    if (!status) {
        Emit(EventType::kStreamError, {}, status.message);
    }
}

Status AudioManager::EnsureMonitoring() {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (monitoring_started_) {
            return Status::Ok();
        }
    }

    const Status status = backend_->StartMonitoring([this](const PipeWireDeviceEvent& event) {
        HandleBackendDeviceEvent(event);
    });
    if (!status) {
        return status;
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    monitoring_started_ = true;
    return Status::Ok();
}

Status AudioManager::ResolveTargetDevice(
    DeviceDirection direction,
    std::string& device_id,
    std::string& device_name,
    bool& used_fallback) {
    const std::vector<DeviceDescriptor> devices = backend_->EnumerateDevices();

    DevicePolicy policy;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (direction == DeviceDirection::kInput) {
            policy = config_.input;
        } else {
            policy = config_.output;
        }
    }

    const ResolvedDeviceChoice choice = ResolveDeviceChoice(devices, direction, policy);
    if (!choice.found) {
        std::ostringstream error;
        error << "No available " << DirectionLabel(direction) << " device matched the configured policy";
        return Status::Error(error.str());
    }

    device_id = choice.device.id;
    device_name = choice.device.name;
    used_fallback = choice.used_fallback;

    bool route_changed = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        std::string& selected_device_id =
            direction == DeviceDirection::kInput ? selected_input_device_id_ : selected_output_device_id_;
        route_changed = selected_device_id != choice.device.id;
        selected_device_id = choice.device.id;
    }

    if (route_changed) {
        Emit(
            EventType::kRouteChanged,
            choice.device.id,
            "Resolved " + DirectionLabel(direction) + " route to " + choice.device.name);
    }
    return Status::Ok();
}

Status AudioManager::StartPlaybackSession(const std::string& path) {
    std::string target_device_id;
    std::string target_device_name;
    bool used_fallback = false;

    if (!std::filesystem::exists(path)) {
        return Status::Error("Recording file does not exist: " + path);
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (playback_active_) {
            return Status::Error("Playback is already active");
        }
    }
    JoinPlaybackWatcher();

    static_cast<void>(EnsureMonitoring());

    Status status = ResolveTargetDevice(DeviceDirection::kOutput, target_device_id, target_device_name, used_fallback);
    if (!status) {
        Emit(EventType::kStreamError, {}, status.message);
        return status;
    }

    if (used_fallback) {
        Emit(
            EventType::kStreamFallbackApplied,
            target_device_id,
            "Playback fell back to " + target_device_name);
    }

    status = backend_->StartPlayback(path, target_device_id);
    if (!status) {
        Emit(EventType::kStreamError, target_device_id, status.message);
        return status;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        playback_active_ = true;
        active_playback_path_ = path;
        active_playback_device_id_ = target_device_id;
    }

    Emit(EventType::kStreamStarted, target_device_id, "Playback started on " + target_device_name);

    playback_watcher_ = std::thread([this, path, target_device_id]() {
        const Status wait_status = backend_->WaitForPlaybackToFinish();

        bool emit_stop = false;
        bool emit_error = false;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (playback_active_ &&
                active_playback_path_ == path &&
                active_playback_device_id_ == target_device_id) {
                playback_active_ = false;
                active_playback_path_.clear();
                active_playback_device_id_.clear();
                emit_stop = wait_status.ok;
                emit_error = !wait_status.ok;
            }
        }

        static_cast<void>(backend_->StopPlayback());

        if (emit_error) {
            Emit(EventType::kStreamError, target_device_id, wait_status.message);
            return;
        }
        if (emit_stop) {
            Emit(EventType::kStreamStopped, target_device_id, "Playback stopped");
        }
    });

    return Status::Ok();
}

Status AudioManager::StopPlaybackSession(bool emit_event) {
    std::string device_id;
    bool was_active = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        was_active = playback_active_;
        if (playback_active_) {
            playback_active_ = false;
            active_playback_path_.clear();
            device_id = active_playback_device_id_;
            active_playback_device_id_.clear();
        }
    }

    if (!was_active) {
        JoinPlaybackWatcher();
        return Status::Error("Playback is not active");
    }

    const Status status = backend_->StopPlayback();
    JoinPlaybackWatcher();
    if (!status) {
        if (emit_event) {
            Emit(EventType::kStreamError, device_id, status.message);
        }
        return status;
    }

    if (emit_event) {
        Emit(EventType::kStreamStopped, device_id, "Playback stopped");
    }
    return Status::Ok();
}

void AudioManager::JoinPlaybackWatcher() {
    if (playback_watcher_.joinable()) {
        playback_watcher_.join();
    }
}

bool AudioManager::IsRecordingSessionActive(const std::string& path) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return recording_active_ && active_recording_path_ == path;
}

bool AudioManager::IsPlaybackSessionActive(const std::string& path) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return playback_active_ && active_playback_path_ == path;
}

void AudioManager::HandleBackendDeviceEvent(const PipeWireDeviceEvent& event) {
    const std::string details = event.device.name.empty() ? event.device.id : event.device.name;
    Emit(event.type, event.device.id, details);

    if (event.type != EventType::kDeviceRemoved) {
        return;
    }

    HandleDeviceRemoval(event.device.direction, event);
}

void AudioManager::HandleDeviceRemoval(DeviceDirection direction, const PipeWireDeviceEvent& event) {
    DevicePolicy policy;
    std::string current_selected_device_id;
    bool active_stream_removed = false;

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (direction == DeviceDirection::kInput) {
            policy = config_.input;
            current_selected_device_id = selected_input_device_id_;
            active_stream_removed = recording_active_ && active_recording_device_id_ == event.device.id;
        } else {
            policy = config_.output;
            current_selected_device_id = selected_output_device_id_;
            active_stream_removed = playback_active_ && active_playback_device_id_ == event.device.id;
        }
    }

    if (current_selected_device_id != event.device.id) {
        if (active_stream_removed) {
            Emit(EventType::kStreamError, event.device.id, "Active " + DirectionLabel(direction) + " stream lost its device");
        }
        return;
    }

    const ResolvedDeviceChoice choice = ResolveDeviceChoice(event.devices, direction, policy);
    if (!choice.found) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (direction == DeviceDirection::kInput) {
                selected_input_device_id_.clear();
            } else {
                selected_output_device_id_.clear();
            }
        }
        Emit(
            EventType::kDeviceAvailableChanged,
            event.device.id,
            "No fallback " + DirectionLabel(direction) + " device is available");
        if (active_stream_removed) {
            Emit(EventType::kStreamError, event.device.id, "Active " + DirectionLabel(direction) + " stream lost its device");
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (direction == DeviceDirection::kInput) {
            selected_input_device_id_ = choice.device.id;
        } else {
            selected_output_device_id_ = choice.device.id;
        }
    }

    std::ostringstream details;
    details << "Switched " << DirectionLabel(direction) << " route from "
            << event.device.name << " to " << choice.device.name;
    Emit(EventType::kStreamFallbackApplied, choice.device.id, details.str());
    Emit(EventType::kRouteChanged, choice.device.id, "Selected " + DirectionLabel(direction) + " fallback route");

    if (active_stream_removed) {
        Emit(
            EventType::kStreamError,
            event.device.id,
            "Active " + DirectionLabel(direction) + " stream lost its device; future streams will use " + choice.device.name);
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
