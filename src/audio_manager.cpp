#include "audio_sdk/audio_manager.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <utility>

#include "backends/pipewire/pipewire_backend.h"

namespace audio_sdk {

namespace {

void WriteLittleEndian(std::ofstream& output, std::uint32_t value) {
    output.put(static_cast<char>(value & 0xFF));
    output.put(static_cast<char>((value >> 8) & 0xFF));
    output.put(static_cast<char>((value >> 16) & 0xFF));
    output.put(static_cast<char>((value >> 24) & 0xFF));
}

void WriteLittleEndian16(std::ofstream& output, std::uint16_t value) {
    output.put(static_cast<char>(value & 0xFF));
    output.put(static_cast<char>((value >> 8) & 0xFF));
}

Status WriteEmptyWav(const std::string& path, int sample_rate, int channels) {
    const std::filesystem::path file_path(path);
    if (!file_path.parent_path().empty()) {
        std::filesystem::create_directories(file_path.parent_path());
    }

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return Status::Error("Failed to create WAV file: " + path);
    }

    constexpr std::uint16_t bits_per_sample = 16;
    const std::uint32_t data_size = 0;
    const std::uint32_t byte_rate = static_cast<std::uint32_t>(sample_rate * channels * bits_per_sample / 8);
    const std::uint16_t block_align = static_cast<std::uint16_t>(channels * bits_per_sample / 8);

    output.write("RIFF", 4);
    WriteLittleEndian(output, 36 + data_size);
    output.write("WAVE", 4);
    output.write("fmt ", 4);
    WriteLittleEndian(output, 16);
    WriteLittleEndian16(output, 1);
    WriteLittleEndian16(output, static_cast<std::uint16_t>(channels));
    WriteLittleEndian(output, static_cast<std::uint32_t>(sample_rate));
    WriteLittleEndian(output, byte_rate);
    WriteLittleEndian16(output, block_align);
    WriteLittleEndian16(output, bits_per_sample);
    output.write("data", 4);
    WriteLittleEndian(output, data_size);

    return Status::Ok();
}

}  // namespace

AudioManager::AudioManager(AudioConfig config)
    : config_(std::move(config)) {}

Status AudioManager::LoadConfigFromFile(const std::string& path) {
    return LoadConfig(path, config_);
}

Status AudioManager::SaveConfigToFile(const std::string& path) const {
    return SaveConfig(path, config_);
}

std::string AudioManager::BackendName() const {
    PipeWireBackend backend;
    return backend.Name();
}

std::vector<DeviceDescriptor> AudioManager::EnumerateDevices() const {
    PipeWireBackend backend;
    return backend.EnumerateDevices();
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

    Status status = WriteEmptyWav(path, config_.stream.sample_rate, config_.stream.channels);
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
    callback_ = std::move(callback);
}

void AudioManager::Emit(EventType type, std::string device_id, std::string details) const {
    if (!callback_) {
        return;
    }

    callback_(Event{type, std::move(device_id), std::move(details)});
}

}  // namespace audio_sdk
