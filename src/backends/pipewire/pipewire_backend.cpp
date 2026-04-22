#include "pipewire_backend.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include <spa/utils/hook.h>
#include <spa/utils/dict.h>
#include <spa/param/audio/format-utils.h>

#include <pipewire/context.h>
#include <pipewire/core.h>
#include <pipewire/keys.h>
#include <pipewire/pipewire.h>
#include <pipewire/properties.h>
#include <pipewire/proxy.h>
#include <pipewire/stream.h>
#include <pipewire/thread-loop.h>

namespace audio_sdk {

namespace {

std::once_flag g_pipewire_init_once;

constexpr std::uint16_t kWavPcmFormat = 1;
constexpr std::uint16_t kWavBitsPerSample = 16;
constexpr std::uint16_t kBytesPerSample = kWavBitsPerSample / 8;
constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

struct PipeWireDeviceRecord {
    DeviceDescriptor descriptor;
    std::string target_object;
};

bool IsAudioNode(const char* media_class) {
    return media_class != nullptr &&
        (std::strcmp(media_class, "Audio/Source") == 0 ||
         std::strcmp(media_class, "Audio/Sink") == 0);
}

DeviceDirection DirectionFromMediaClass(const char* media_class) {
    return std::strcmp(media_class, "Audio/Sink") == 0
        ? DeviceDirection::kOutput
        : DeviceDirection::kInput;
}

std::string LookupProperty(const struct spa_dict* props, const char* key) {
    const char* value = props == nullptr ? nullptr : spa_dict_lookup(props, key);
    return value == nullptr ? std::string{} : std::string(value);
}

std::string FallbackNodeId(uint32_t global_id) {
    std::ostringstream stream;
    stream << "pipewire-node-" << global_id;
    return stream.str();
}

std::string BuildDeviceName(const struct spa_dict* props, uint32_t global_id) {
    std::string name = LookupProperty(props, PW_KEY_NODE_DESCRIPTION);
    if (!name.empty()) {
        return name;
    }

    name = LookupProperty(props, PW_KEY_NODE_NICK);
    if (!name.empty()) {
        return name;
    }

    name = LookupProperty(props, PW_KEY_NODE_NAME);
    if (!name.empty()) {
        return name;
    }

    return FallbackNodeId(global_id);
}

void AppendIdentityProperty(
    const struct spa_dict* props,
    const char* key,
    std::ostringstream& identity,
    bool& has_stable_property) {
    const std::string value = LookupProperty(props, key);
    if (value.empty()) {
        return;
    }

    identity << key << '=' << value << '\n';
    has_stable_property = true;
}

std::string BuildStableIdentity(
    const struct spa_dict* props,
    uint32_t global_id,
    DeviceDirection direction) {
    std::ostringstream identity;
    identity << "direction=" << (direction == DeviceDirection::kInput ? "input" : "output") << '\n';

    bool has_stable_property = false;
    AppendIdentityProperty(props, PW_KEY_DEVICE_SERIAL, identity, has_stable_property);
    AppendIdentityProperty(props, PW_KEY_DEVICE_BUS_PATH, identity, has_stable_property);
    AppendIdentityProperty(props, PW_KEY_DEVICE_NAME, identity, has_stable_property);
    AppendIdentityProperty(props, PW_KEY_DEVICE_STRING, identity, has_stable_property);
    AppendIdentityProperty(props, PW_KEY_DEVICE_VENDOR_ID, identity, has_stable_property);
    AppendIdentityProperty(props, PW_KEY_DEVICE_PRODUCT_ID, identity, has_stable_property);
    AppendIdentityProperty(props, PW_KEY_OBJECT_PATH, identity, has_stable_property);
    AppendIdentityProperty(props, PW_KEY_NODE_NAME, identity, has_stable_property);
    AppendIdentityProperty(props, PW_KEY_NODE_DESCRIPTION, identity, has_stable_property);
    AppendIdentityProperty(props, "api.alsa.path", identity, has_stable_property);
    AppendIdentityProperty(props, "api.alsa.card.name", identity, has_stable_property);
    AppendIdentityProperty(props, "api.alsa.pcm.device", identity, has_stable_property);
    AppendIdentityProperty(props, "api.alsa.pcm.name", identity, has_stable_property);

    if (!has_stable_property) {
        identity << "pipewire_global_id=" << global_id << '\n';
    }
    return identity.str();
}

std::string ShortDeviceId(DeviceDirection direction, const std::string& identity) {
    std::uint64_t hash = kFnvOffsetBasis;
    for (const unsigned char ch : identity) {
        hash ^= ch;
        hash *= kFnvPrime;
    }

    std::ostringstream stream;
    stream << (direction == DeviceDirection::kInput ? "in-" : "out-")
           << std::hex << std::nouppercase << std::setfill('0') << std::setw(10)
           << (hash & 0xffffffffffull);
    return stream.str();
}

bool MakeDeviceRecord(uint32_t global_id, const struct spa_dict* props, PipeWireDeviceRecord& record) {
    const char* media_class = props == nullptr ? nullptr : spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (!IsAudioNode(media_class)) {
        return false;
    }

    const DeviceDirection direction = DirectionFromMediaClass(media_class);
    const std::string node_name = LookupProperty(props, PW_KEY_NODE_NAME);
    const std::string identity = BuildStableIdentity(props, global_id, direction);

    record.descriptor.id = ShortDeviceId(direction, identity);
    record.descriptor.name = BuildDeviceName(props, global_id);
    record.descriptor.direction = direction;
    record.descriptor.is_default = false;
    record.descriptor.available = true;
    record.target_object = node_name.empty() ? FallbackNodeId(global_id) : node_name;
    return true;
}

std::vector<DeviceDescriptor> SortedDevices(const std::unordered_map<uint32_t, PipeWireDeviceRecord>& devices) {
    std::vector<DeviceDescriptor> results;
    results.reserve(devices.size());
    for (const auto& entry : devices) {
        results.push_back(entry.second.descriptor);
    }

    std::sort(results.begin(), results.end(), [](const DeviceDescriptor& left, const DeviceDescriptor& right) {
        if (left.direction != right.direction) {
            return left.direction == DeviceDirection::kInput;
        }
        if (left.name != right.name) {
            return left.name < right.name;
        }
        return left.id < right.id;
    });

    return results;
}

void WriteLittleEndian16(std::ostream& output, std::uint16_t value) {
    output.put(static_cast<char>(value & 0xFF));
    output.put(static_cast<char>((value >> 8) & 0xFF));
}

void WriteLittleEndian32(std::ostream& output, std::uint32_t value) {
    output.put(static_cast<char>(value & 0xFF));
    output.put(static_cast<char>((value >> 8) & 0xFF));
    output.put(static_cast<char>((value >> 16) & 0xFF));
    output.put(static_cast<char>((value >> 24) & 0xFF));
}

std::uint16_t ReadLittleEndian16(std::istream& input) {
    const int low = input.get();
    const int high = input.get();
    if (low == EOF || high == EOF) {
        return 0;
    }
    return static_cast<std::uint16_t>(low | (high << 8));
}

std::uint32_t ReadLittleEndian32(std::istream& input) {
    const int byte0 = input.get();
    const int byte1 = input.get();
    const int byte2 = input.get();
    const int byte3 = input.get();
    if (byte0 == EOF || byte1 == EOF || byte2 == EOF || byte3 == EOF) {
        return 0;
    }
    return static_cast<std::uint32_t>(
        byte0 |
        (byte1 << 8) |
        (byte2 << 16) |
        (byte3 << 24));
}

Status WriteWavHeader(std::ostream& output, int sample_rate, int channels, std::uint32_t data_size) {
    if (sample_rate <= 0) {
        return Status::Error("Invalid sample rate for WAV header");
    }
    if (channels <= 0 || channels > 2) {
        return Status::Error("Only mono and stereo WAV output are supported");
    }

    const std::uint32_t byte_rate = static_cast<std::uint32_t>(sample_rate * channels * kBytesPerSample);
    const std::uint16_t block_align = static_cast<std::uint16_t>(channels * kBytesPerSample);

    output.seekp(0);
    output.write("RIFF", 4);
    WriteLittleEndian32(output, 36 + data_size);
    output.write("WAVE", 4);
    output.write("fmt ", 4);
    WriteLittleEndian32(output, 16);
    WriteLittleEndian16(output, kWavPcmFormat);
    WriteLittleEndian16(output, static_cast<std::uint16_t>(channels));
    WriteLittleEndian32(output, static_cast<std::uint32_t>(sample_rate));
    WriteLittleEndian32(output, byte_rate);
    WriteLittleEndian16(output, block_align);
    WriteLittleEndian16(output, kWavBitsPerSample);
    output.write("data", 4);
    WriteLittleEndian32(output, data_size);
    return Status::Ok();
}

void ApplyChannelLayout(spa_audio_info_raw& info) {
    if (info.channels == 1) {
        info.position[0] = SPA_AUDIO_CHANNEL_MONO;
        return;
    }
    if (info.channels == 2) {
        info.position[0] = SPA_AUDIO_CHANNEL_FL;
        info.position[1] = SPA_AUDIO_CHANNEL_FR;
        return;
    }
    info.flags |= SPA_AUDIO_FLAG_UNPOSITIONED;
}

Status BuildRawAudioFormat(
    std::uint8_t* buffer,
    std::size_t buffer_size,
    int sample_rate,
    int channels,
    const spa_pod*& out_param) {
    if (sample_rate <= 0) {
        return Status::Error("Sample rate must be positive");
    }
    if (channels <= 0 || channels > 2) {
        return Status::Error("Only mono and stereo PipeWire streams are supported right now");
    }

    spa_audio_info_raw info = SPA_AUDIO_INFO_RAW_INIT(
        .format = SPA_AUDIO_FORMAT_S16,
        .rate = static_cast<std::uint32_t>(sample_rate),
        .channels = static_cast<std::uint32_t>(channels));
    ApplyChannelLayout(info);

    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, static_cast<std::uint32_t>(buffer_size));
    out_param = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info);
    return Status::Ok();
}

struct WavFileInfo {
    int sample_rate = 0;
    int channels = 0;
    std::uint32_t data_offset = 0;
    std::uint32_t data_size = 0;
};

Status ParseWavFile(const std::string& path, WavFileInfo& info) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Status::Error("Failed to open WAV file: " + path);
    }

    char riff[4] {};
    char wave[4] {};
    input.read(riff, 4);
    ReadLittleEndian32(input);
    input.read(wave, 4);

    if (std::strncmp(riff, "RIFF", 4) != 0 || std::strncmp(wave, "WAVE", 4) != 0) {
        return Status::Error("Unsupported WAV container: " + path);
    }

    bool saw_fmt = false;
    bool saw_data = false;

    while (input && (!saw_fmt || !saw_data)) {
        char chunk_id[4] {};
        input.read(chunk_id, 4);
        if (input.gcount() != 4) {
            break;
        }

        const std::uint32_t chunk_size = ReadLittleEndian32(input);
        const std::streampos chunk_data_start = input.tellg();

        if (std::strncmp(chunk_id, "fmt ", 4) == 0) {
            const std::uint16_t audio_format = ReadLittleEndian16(input);
            const std::uint16_t channels = ReadLittleEndian16(input);
            const std::uint32_t sample_rate = ReadLittleEndian32(input);
            ReadLittleEndian32(input);
            ReadLittleEndian16(input);
            const std::uint16_t bits_per_sample = ReadLittleEndian16(input);

            if (audio_format != kWavPcmFormat || bits_per_sample != kWavBitsPerSample) {
                return Status::Error("Only PCM16 WAV playback is supported right now");
            }
            if (channels == 0 || channels > 2) {
                return Status::Error("Only mono and stereo WAV playback is supported right now");
            }

            info.channels = channels;
            info.sample_rate = static_cast<int>(sample_rate);
            saw_fmt = true;
        } else if (std::strncmp(chunk_id, "data", 4) == 0) {
            info.data_offset = static_cast<std::uint32_t>(chunk_data_start);
            info.data_size = chunk_size;
            saw_data = true;
        }

        const std::uint32_t padded_size = chunk_size + (chunk_size & 1u);
        input.seekg(chunk_data_start + static_cast<std::streamoff>(padded_size));
    }

    if (!saw_fmt || !saw_data) {
        return Status::Error("Incomplete WAV file: " + path);
    }
    return Status::Ok();
}

}  // namespace

class PipeWireBackend::RegistrySession {
  public:
    RegistrySession() = default;

    ~RegistrySession() {
        Disconnect();
    }

    Status Connect() {
        loop_ = pw_thread_loop_new("audio_sdk-pipewire", nullptr);
        if (loop_ == nullptr) {
            return Status::Error("Failed to create PipeWire thread loop");
        }

        if (pw_thread_loop_start(loop_) < 0) {
            pw_thread_loop_destroy(loop_);
            loop_ = nullptr;
            return Status::Error("Failed to start PipeWire thread loop");
        }
        loop_started_ = true;

        pw_thread_loop_lock(loop_);

        context_ = pw_context_new(
            pw_thread_loop_get_loop(loop_),
            pw_properties_new(
                PW_KEY_APP_NAME, "audio_sdk",
                PW_KEY_APP_ID, "io.github.s-egk.audio_sdk",
                PW_KEY_CLIENT_NAME, "audio_sdk",
                nullptr),
            0);
        if (context_ == nullptr) {
            return FailLocked("Failed to create PipeWire context");
        }

        core_ = pw_context_connect(context_, nullptr, 0);
        if (core_ == nullptr) {
            return FailLocked("Failed to connect to PipeWire: " + std::string(std::strerror(errno)));
        }

        static const pw_core_events kCoreEvents = {
            PW_VERSION_CORE_EVENTS,
            nullptr,
            &RegistrySession::OnCoreDone,
            nullptr,
            &RegistrySession::OnCoreError,
        };

        static const pw_registry_events kRegistryEvents = {
            PW_VERSION_REGISTRY_EVENTS,
            &RegistrySession::OnRegistryGlobal,
            &RegistrySession::OnRegistryGlobalRemove,
        };

        pw_core_add_listener(core_, &core_listener_, &kCoreEvents, this);
        core_listener_attached_ = true;

        registry_ = pw_core_get_registry(core_, PW_VERSION_REGISTRY, 0);
        if (registry_ == nullptr) {
            return FailLocked("Failed to acquire PipeWire registry");
        }

        pw_registry_add_listener(registry_, &registry_listener_, &kRegistryEvents, this);
        registry_listener_attached_ = true;

        pending_sync_seq_ = pw_core_sync(core_, PW_ID_CORE, 0);
        while (!initial_sync_complete_ && error_message_.empty()) {
            pw_thread_loop_wait(loop_);
        }

        const std::string error_message = error_message_;
        pw_thread_loop_unlock(loop_);

        if (!error_message.empty()) {
            Disconnect();
            return Status::Error(error_message);
        }

        return Status::Ok();
    }

    void SetCallback(std::function<void(const PipeWireDeviceEvent&)> callback) {
        pw_thread_loop_lock(loop_);
        callback_ = std::move(callback);
        emit_device_events_ = static_cast<bool>(callback_);
        pw_thread_loop_unlock(loop_);
    }

    std::vector<DeviceDescriptor> Devices() const {
        if (loop_ == nullptr) {
            return {};
        }

        pw_thread_loop_lock(loop_);
        const auto results = SortedDevices(devices_);
        pw_thread_loop_unlock(loop_);
        return results;
    }

    std::string TargetObjectForDeviceId(const std::string& device_id) const {
        if (loop_ == nullptr || device_id.empty()) {
            return {};
        }

        pw_thread_loop_lock(loop_);
        std::string target_object;
        for (const auto& entry : devices_) {
            if (entry.second.descriptor.id == device_id) {
                target_object = entry.second.target_object;
                break;
            }
        }
        pw_thread_loop_unlock(loop_);
        return target_object;
    }

    void Disconnect() {
        if (loop_ == nullptr) {
            return;
        }

        pw_thread_loop_lock(loop_);

        callback_ = nullptr;
        emit_device_events_ = false;
        ClearPipeWireObjectsLocked();

        pw_thread_loop_unlock(loop_);
        if (loop_started_) {
            pw_thread_loop_stop(loop_);
            loop_started_ = false;
        }
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
    }

  private:
    Status FailLocked(const std::string& message) {
        ClearPipeWireObjectsLocked();
        pw_thread_loop_unlock(loop_);
        if (loop_started_) {
            pw_thread_loop_stop(loop_);
            loop_started_ = false;
        }
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
        return Status::Error(message);
    }

    void ClearPipeWireObjectsLocked() {
        if (registry_listener_attached_) {
            spa_hook_remove(&registry_listener_);
            registry_listener_attached_ = false;
        }
        if (registry_ != nullptr) {
            pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry_));
            registry_ = nullptr;
        }

        if (core_listener_attached_) {
            spa_hook_remove(&core_listener_);
            core_listener_attached_ = false;
        }
        if (core_ != nullptr) {
            pw_core_disconnect(core_);
            core_ = nullptr;
        }

        if (context_ != nullptr) {
            pw_context_destroy(context_);
            context_ = nullptr;
        }

        devices_.clear();
        pending_sync_seq_ = -1;
        initial_sync_complete_ = false;
        error_message_.clear();
    }

    static void OnCoreDone(void* data, uint32_t id, int seq) {
        auto* self = static_cast<RegistrySession*>(data);
        if (id != PW_ID_CORE || seq != self->pending_sync_seq_) {
            return;
        }

        self->initial_sync_complete_ = true;
        pw_thread_loop_signal(self->loop_, false);
    }

    static void OnCoreError(void* data, uint32_t id, int seq, int res, const char* message) {
        auto* self = static_cast<RegistrySession*>(data);
        std::ostringstream stream;
        stream << "PipeWire error";
        if (message != nullptr && message[0] != '\0') {
            stream << ": " << message;
        } else if (res != 0) {
            stream << ": " << std::strerror(-res);
        }

        self->error_message_ = stream.str();
        self->initial_sync_complete_ = true;
        pw_thread_loop_signal(self->loop_, false);
    }

    static void OnRegistryGlobal(
        void* data,
        uint32_t id,
        uint32_t permissions,
        const char* type,
        uint32_t version,
        const struct spa_dict* props) {
        static_cast<void>(permissions);
        static_cast<void>(version);

        auto* self = static_cast<RegistrySession*>(data);
        if (type == nullptr || std::strcmp(type, PW_TYPE_INTERFACE_Node) != 0) {
            return;
        }

        PipeWireDeviceRecord record;
        if (!MakeDeviceRecord(id, props, record)) {
            return;
        }

        self->devices_[id] = record;
        if (!self->initial_sync_complete_ || !self->emit_device_events_ || !self->callback_) {
            return;
        }

        self->callback_(PipeWireDeviceEvent{
            EventType::kDeviceAdded,
            record.descriptor,
            SortedDevices(self->devices_)});
    }

    static void OnRegistryGlobalRemove(void* data, uint32_t id) {
        auto* self = static_cast<RegistrySession*>(data);
        auto device = self->devices_.find(id);
        if (device == self->devices_.end()) {
            return;
        }

        const DeviceDescriptor removed_device = device->second.descriptor;
        self->devices_.erase(device);

        if (!self->initial_sync_complete_ || !self->emit_device_events_ || !self->callback_) {
            return;
        }

        self->callback_(PipeWireDeviceEvent{EventType::kDeviceRemoved, removed_device, SortedDevices(self->devices_)});
    }

    pw_thread_loop* loop_ = nullptr;
    pw_context* context_ = nullptr;
    pw_core* core_ = nullptr;
    pw_registry* registry_ = nullptr;
    spa_hook core_listener_ {};
    spa_hook registry_listener_ {};
    bool core_listener_attached_ = false;
    bool registry_listener_attached_ = false;
    std::unordered_map<uint32_t, PipeWireDeviceRecord> devices_;
    std::function<void(const PipeWireDeviceEvent&)> callback_;
    int pending_sync_seq_ = -1;
    bool initial_sync_complete_ = false;
    bool emit_device_events_ = false;
    bool loop_started_ = false;
    std::string error_message_;
};

class PipeWireBackend::RecordingSession {
  public:
    ~RecordingSession() {
        Stop();
    }

    Status Start(const std::string& path, const std::string& target_device_id, const StreamPolicy& policy) {
        if (loop_ != nullptr) {
            return Status::Error("Recording session is already active");
        }

        path_ = path;
        sample_rate_ = policy.sample_rate;
        channels_ = policy.channels;

        const std::filesystem::path file_path(path);
        if (!file_path.parent_path().empty()) {
            std::filesystem::create_directories(file_path.parent_path());
        }

        output_.open(path, std::ios::binary | std::ios::trunc);
        if (!output_) {
            return Status::Error("Failed to open recording file: " + path);
        }

        Status status = WriteWavHeader(output_, sample_rate_, channels_, 0);
        if (!status) {
            output_.close();
            return status;
        }

        loop_ = pw_thread_loop_new("audio_sdk-recorder", nullptr);
        if (loop_ == nullptr) {
            output_.close();
            std::filesystem::remove(path_);
            return Status::Error("Failed to create recording loop");
        }
        if (pw_thread_loop_start(loop_) < 0) {
            pw_thread_loop_destroy(loop_);
            loop_ = nullptr;
            output_.close();
            std::filesystem::remove(path_);
            return Status::Error("Failed to start recording loop");
        }

        pw_thread_loop_lock(loop_);

        struct pw_properties* props = pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Production",
            PW_KEY_APP_NAME, "audio_sdk",
            nullptr);
        if (!target_device_id.empty()) {
            pw_properties_set(props, PW_KEY_TARGET_OBJECT, target_device_id.c_str());
        }

        stream_ = pw_stream_new_simple(
            pw_thread_loop_get_loop(loop_),
            "audio_sdk-recorder",
            props,
            &kStreamEvents,
            this);
        if (stream_ == nullptr) {
            return FailLocked("Failed to create PipeWire recording stream");
        }

        std::uint8_t buffer[1024] {};
        const struct spa_pod* params[1] {};
        status = BuildRawAudioFormat(buffer, sizeof(buffer), sample_rate_, channels_, params[0]);
        if (!status) {
            return FailLocked(status.message);
        }

        if (pw_stream_connect(
                stream_,
                PW_DIRECTION_INPUT,
                PW_ID_ANY,
                static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
                params,
                1) < 0) {
            return FailLocked("Failed to connect PipeWire recording stream");
        }

        while (!ready_ && error_message_.empty()) {
            pw_thread_loop_wait(loop_);
        }

        const std::string error_message = error_message_;
        pw_thread_loop_unlock(loop_);

        if (!error_message.empty()) {
            Stop();
            std::filesystem::remove(path_);
            return Status::Error(error_message);
        }

        return Status::Ok();
    }

    Status Stop() {
        if (loop_ == nullptr) {
            return Status::Error("Recording session is not active");
        }

        pw_thread_loop_lock(loop_);
        if (stream_ != nullptr) {
            pw_stream_disconnect(stream_);
            pw_stream_destroy(stream_);
            stream_ = nullptr;
        }
        pw_thread_loop_unlock(loop_);

        pw_thread_loop_stop(loop_);
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;

        if (output_.is_open()) {
            Status status = WriteWavHeader(output_, sample_rate_, channels_, bytes_written_);
            output_.flush();
            output_.close();
            if (!status) {
                return status;
            }
        }

        return Status::Ok();
    }

  private:
    Status FailLocked(const std::string& message) {
        error_message_ = message;
        if (stream_ != nullptr) {
            pw_stream_destroy(stream_);
            stream_ = nullptr;
        }
        pw_thread_loop_unlock(loop_);
        pw_thread_loop_stop(loop_);
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
        if (output_.is_open()) {
            output_.close();
        }
        return Status::Error(message);
    }

    static void OnStateChanged(void* data, pw_stream_state old_state, pw_stream_state state, const char* error) {
        static_cast<void>(old_state);
        auto* self = static_cast<RecordingSession*>(data);
        if (state == PW_STREAM_STATE_PAUSED || state == PW_STREAM_STATE_STREAMING) {
            self->ready_ = true;
            pw_thread_loop_signal(self->loop_, false);
            return;
        }

        if (state == PW_STREAM_STATE_ERROR) {
            self->error_message_ = error == nullptr ? "PipeWire recording stream failed" : error;
            self->ready_ = true;
            pw_thread_loop_signal(self->loop_, false);
        }
    }

    static void OnProcess(void* data) {
        auto* self = static_cast<RecordingSession*>(data);
        if (self->stream_ == nullptr || !self->output_.is_open()) {
            return;
        }

        while (auto* buffer = pw_stream_dequeue_buffer(self->stream_)) {
            auto* spa_buffer = buffer->buffer;
            if (spa_buffer->n_datas == 0 || spa_buffer->datas[0].data == nullptr || spa_buffer->datas[0].chunk == nullptr) {
                pw_stream_queue_buffer(self->stream_, buffer);
                continue;
            }

            auto& data_area = spa_buffer->datas[0];
            const std::uint32_t offset = std::min(data_area.chunk->offset, data_area.maxsize);
            const std::uint32_t available = offset <= data_area.maxsize ? (data_area.maxsize - offset) : 0;
            const std::uint32_t size = std::min(data_area.chunk->size, available);
            const auto* source = static_cast<const char*>(data_area.data) + offset;

            if (size > 0) {
                self->output_.write(source, size);
                self->bytes_written_ += size;
            }

            pw_stream_queue_buffer(self->stream_, buffer);
        }
    }

    static const pw_stream_events kStreamEvents;

    pw_thread_loop* loop_ = nullptr;
    pw_stream* stream_ = nullptr;
    std::ofstream output_;
    std::string path_;
    int sample_rate_ = 0;
    int channels_ = 0;
    std::uint32_t bytes_written_ = 0;
    bool ready_ = false;
    std::string error_message_;
};

const pw_stream_events PipeWireBackend::RecordingSession::kStreamEvents = {
    PW_VERSION_STREAM_EVENTS,
    nullptr,
    &PipeWireBackend::RecordingSession::OnStateChanged,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &PipeWireBackend::RecordingSession::OnProcess,
    nullptr,
};

class PipeWireBackend::PlaybackSession {
  public:
    ~PlaybackSession() {
        Stop();
    }

    Status Start(const std::string& path, const std::string& target_device_id) {
        if (loop_ != nullptr) {
            return Status::Error("Playback session is already active");
        }

        Status status = ParseWavFile(path, wav_info_);
        if (!status) {
            return status;
        }

        input_.open(path, std::ios::binary);
        if (!input_) {
            return Status::Error("Failed to open recording for playback: " + path);
        }
        input_.seekg(static_cast<std::streamoff>(wav_info_.data_offset));
        bytes_remaining_ = wav_info_.data_size;

        loop_ = pw_thread_loop_new("audio_sdk-playback", nullptr);
        if (loop_ == nullptr) {
            return Status::Error("Failed to create playback loop");
        }
        if (pw_thread_loop_start(loop_) < 0) {
            pw_thread_loop_destroy(loop_);
            loop_ = nullptr;
            return Status::Error("Failed to start playback loop");
        }

        pw_thread_loop_lock(loop_);

        struct pw_properties* props = pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Playback",
            PW_KEY_MEDIA_ROLE, "Music",
            PW_KEY_APP_NAME, "audio_sdk",
            nullptr);
        if (!target_device_id.empty()) {
            pw_properties_set(props, PW_KEY_TARGET_OBJECT, target_device_id.c_str());
        }

        stream_ = pw_stream_new_simple(
            pw_thread_loop_get_loop(loop_),
            "audio_sdk-player",
            props,
            &kStreamEvents,
            this);
        if (stream_ == nullptr) {
            return FailLocked("Failed to create PipeWire playback stream");
        }

        std::uint8_t buffer[1024] {};
        const struct spa_pod* params[1] {};
        status = BuildRawAudioFormat(buffer, sizeof(buffer), wav_info_.sample_rate, wav_info_.channels, params[0]);
        if (!status) {
            return FailLocked(status.message);
        }

        if (pw_stream_connect(
                stream_,
                PW_DIRECTION_OUTPUT,
                PW_ID_ANY,
                static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
                params,
                1) < 0) {
            return FailLocked("Failed to connect PipeWire playback stream");
        }

        while (!ready_ && error_message_.empty()) {
            pw_thread_loop_wait(loop_);
        }

        const std::string error_message = error_message_;
        pw_thread_loop_unlock(loop_);

        if (!error_message.empty()) {
            Stop();
            return Status::Error(error_message);
        }
        return Status::Ok();
    }

    Status WaitUntilFinished() {
        if (loop_ == nullptr) {
            return Status::Error("Playback session is not active");
        }

        pw_thread_loop_lock(loop_);
        while (error_message_.empty() && !finished_) {
            pw_thread_loop_wait(loop_);
        }
        const std::string error_message = error_message_;
        pw_thread_loop_unlock(loop_);

        if (!error_message.empty()) {
            return Status::Error(error_message);
        }
        return Status::Ok();
    }

    Status Stop() {
        if (loop_ == nullptr) {
            return Status::Error("Playback session is not active");
        }

        pw_thread_loop_lock(loop_);
        stop_requested_ = true;
        finished_ = true;
        pw_thread_loop_signal(loop_, false);
        TeardownLocked();
        pw_thread_loop_unlock(loop_);

        pw_thread_loop_stop(loop_);
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
        input_.close();

        return Status::Ok();
    }

  private:
    Status FailLocked(const std::string& message) {
        error_message_ = message;
        finished_ = true;
        pw_thread_loop_signal(loop_, false);
        TeardownLocked();
        pw_thread_loop_unlock(loop_);
        pw_thread_loop_stop(loop_);
        pw_thread_loop_destroy(loop_);
        loop_ = nullptr;
        if (input_.is_open()) {
            input_.close();
        }
        return Status::Error(message);
    }

    void TeardownLocked() {
        if (stream_ != nullptr) {
            pw_stream_disconnect(stream_);
            pw_stream_destroy(stream_);
            stream_ = nullptr;
        }
    }

    static void OnStateChanged(void* data, pw_stream_state old_state, pw_stream_state state, const char* error) {
        static_cast<void>(old_state);
        auto* self = static_cast<PipeWireBackend::PlaybackSession*>(data);
        if (state == PW_STREAM_STATE_PAUSED || state == PW_STREAM_STATE_STREAMING) {
            self->ready_ = true;
            pw_thread_loop_signal(self->loop_, false);
            return;
        }
        if (state == PW_STREAM_STATE_ERROR) {
            self->error_message_ = error == nullptr ? "PipeWire playback stream failed" : error;
            self->ready_ = true;
            self->finished_ = true;
            pw_thread_loop_signal(self->loop_, false);
        }
    }

    static void OnProcess(void* data) {
        auto* self = static_cast<PipeWireBackend::PlaybackSession*>(data);
        if (self->stream_ == nullptr || self->drain_requested_ || self->stop_requested_) {
            return;
        }

        while (self->bytes_remaining_ > 0) {
            auto* buffer = pw_stream_dequeue_buffer(self->stream_);
            if (buffer == nullptr) {
                break;
            }

            auto* spa_buffer = buffer->buffer;
            if (spa_buffer->n_datas == 0 || spa_buffer->datas[0].data == nullptr || spa_buffer->datas[0].chunk == nullptr) {
                if (spa_buffer->n_datas > 0 && spa_buffer->datas[0].chunk != nullptr) {
                    spa_buffer->datas[0].chunk->offset = 0;
                    spa_buffer->datas[0].chunk->size = 0;
                    spa_buffer->datas[0].chunk->stride = 0;
                }
                buffer->size = 0;
                pw_stream_queue_buffer(self->stream_, buffer);
                break;
            }

            auto& data_area = spa_buffer->datas[0];
            const std::uint32_t max_size = data_area.maxsize;
            const std::uint32_t bytes_to_copy = std::min(self->bytes_remaining_, max_size);

            self->input_.read(static_cast<char*>(data_area.data), bytes_to_copy);
            const std::streamsize bytes_read = self->input_.gcount();
            const std::uint32_t copied = bytes_read < 0 ? 0u : static_cast<std::uint32_t>(bytes_read);

            data_area.chunk->offset = 0;
            data_area.chunk->size = copied;
            data_area.chunk->stride = self->wav_info_.channels * static_cast<int>(kBytesPerSample);
            buffer->size = copied / static_cast<std::uint64_t>(self->wav_info_.channels * kBytesPerSample);

            self->bytes_remaining_ -= copied;
            pw_stream_queue_buffer(self->stream_, buffer);

            if (copied == 0) {
                self->bytes_remaining_ = 0;
                break;
            }
        }

        if (self->bytes_remaining_ == 0 && !self->drain_requested_) {
            self->drain_requested_ = true;
            pw_stream_flush(self->stream_, true);
        }
    }

    static void OnDrained(void* data) {
        auto* self = static_cast<PipeWireBackend::PlaybackSession*>(data);
        self->finished_ = true;
        pw_thread_loop_signal(self->loop_, false);
    }

    static const pw_stream_events kStreamEvents;

    pw_thread_loop* loop_ = nullptr;
    pw_stream* stream_ = nullptr;
    std::ifstream input_;
    WavFileInfo wav_info_;
    std::uint32_t bytes_remaining_ = 0;
    bool ready_ = false;
    bool stop_requested_ = false;
    bool drain_requested_ = false;
    bool finished_ = false;
    std::string error_message_;
};

const pw_stream_events PipeWireBackend::PlaybackSession::kStreamEvents = {
    PW_VERSION_STREAM_EVENTS,
    nullptr,
    &PipeWireBackend::PlaybackSession::OnStateChanged,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &PipeWireBackend::PlaybackSession::OnProcess,
    &PipeWireBackend::PlaybackSession::OnDrained,
};

PipeWireBackend::PipeWireBackend() {
    std::call_once(g_pipewire_init_once, []() {
        pw_init(nullptr, nullptr);
    });
}

PipeWireBackend::~PipeWireBackend() {
    if (recorder_ != nullptr) {
        recorder_->Stop();
        delete recorder_;
        recorder_ = nullptr;
    }
    if (player_ != nullptr) {
        player_->Stop();
        delete player_;
        player_ = nullptr;
    }
    StopMonitoring();
}

std::string PipeWireBackend::Name() const {
    return "pipewire";
}

std::vector<DeviceDescriptor> PipeWireBackend::EnumerateDevices() const {
    if (monitor_ != nullptr) {
        return monitor_->Devices();
    }

    RegistrySession snapshot;
    const Status status = snapshot.Connect();
    if (!status) {
        return {};
    }

    return snapshot.Devices();
}

std::string PipeWireBackend::ResolveTargetObject(const std::string& device_id) const {
    if (device_id.empty()) {
        return {};
    }

    if (monitor_ != nullptr) {
        const std::string target_object = monitor_->TargetObjectForDeviceId(device_id);
        if (!target_object.empty()) {
            return target_object;
        }
    }

    return device_id;
}

Status PipeWireBackend::StartMonitoring(std::function<void(const PipeWireDeviceEvent&)> callback) {
    callback_ = std::move(callback);

    if (monitor_ == nullptr) {
        auto session = std::make_unique<RegistrySession>();
        const Status status = session->Connect();
        if (!status) {
            callback_ = nullptr;
            return status;
        }

        monitor_ = session.release();
    }

    monitor_->SetCallback(callback_);
    return Status::Ok();
}

void PipeWireBackend::StopMonitoring() {
    callback_ = nullptr;
    if (monitor_ == nullptr) {
        return;
    }

    monitor_->Disconnect();
    delete monitor_;
    monitor_ = nullptr;
}

Status PipeWireBackend::StartRecording(const std::string& path, const std::string& target_device_id, const StreamPolicy& policy) {
    if (recorder_ != nullptr) {
        return Status::Error("Recording session is already active");
    }

    auto session = std::make_unique<RecordingSession>();
    const Status status = session->Start(path, ResolveTargetObject(target_device_id), policy);
    if (!status) {
        return status;
    }

    recorder_ = session.release();
    return Status::Ok();
}

Status PipeWireBackend::StopRecording() {
    if (recorder_ == nullptr) {
        return Status::Error("Recording session is not active");
    }

    const Status status = recorder_->Stop();
    delete recorder_;
    recorder_ = nullptr;
    return status;
}

Status PipeWireBackend::StartPlayback(const std::string& path, const std::string& target_device_id) {
    if (player_ != nullptr) {
        return Status::Error("Playback session is already active");
    }

    auto session = std::make_unique<PlaybackSession>();
    const Status status = session->Start(path, ResolveTargetObject(target_device_id));
    if (!status) {
        return status;
    }

    player_ = session.release();
    return Status::Ok();
}

Status PipeWireBackend::WaitForPlaybackToFinish() {
    if (player_ == nullptr) {
        return Status::Error("Playback session is not active");
    }

    return player_->WaitUntilFinished();
}

Status PipeWireBackend::StopPlayback() {
    if (player_ == nullptr) {
        return Status::Error("Playback session is not active");
    }

    const Status status = player_->Stop();
    delete player_;
    player_ = nullptr;
    return status;
}

Status PipeWireBackend::PlayRecording(const std::string& path, const std::string& target_device_id) {
    const Status start_status = StartPlayback(path, target_device_id);
    if (!start_status) {
        return start_status;
    }

    const Status wait_status = WaitForPlaybackToFinish();
    const Status stop_status = StopPlayback();
    if (!wait_status) {
        return wait_status;
    }
    return stop_status;
}

}  // namespace audio_sdk
