#pragma once

#include <string>

#include "audio_sdk/status.h"

namespace audio_sdk {

struct DevicePolicy {
    std::string preferred_id;
    std::string preferred_name;
    std::string fallback_id;
    std::string fallback_name;
};

struct RecordingPolicy {
    std::string output_path = "recordings/last_capture.wav";
    bool delete_after_playback = false;
};

struct StreamPolicy {
    int sample_rate = 48000;
    int channels = 2;
    bool strict_sync = true;
};

struct AudioConfig {
    std::string backend = "pipewire";
    DevicePolicy input;
    DevicePolicy output;
    RecordingPolicy recording;
    StreamPolicy stream;
};

Status LoadConfig(const std::string& path, AudioConfig& config);
Status SaveConfig(const std::string& path, const AudioConfig& config);

}  // namespace audio_sdk

