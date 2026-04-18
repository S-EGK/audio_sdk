#pragma once

#include <functional>
#include <string>

#include "audio_sdk/status.h"

namespace audio_sdk {

enum class AudioSessionMode {
    kRecording,
    kPlayback,
};

class AudioSession {
  public:
    ~AudioSession();

    AudioSession(AudioSession&& other) noexcept;
    AudioSession& operator=(AudioSession&& other) noexcept;

    AudioSession(const AudioSession&) = delete;
    AudioSession& operator=(const AudioSession&) = delete;

    Status Start();
    Status Stop();
    bool active() const;

    AudioSessionMode mode() const { return mode_; }
    const std::string& path() const { return path_; }

  private:
    friend class AudioManager;

    AudioSession(
        AudioSessionMode mode,
        std::string path,
        std::function<Status()> start_fn,
        std::function<Status()> stop_fn,
        std::function<bool()> active_fn);

    void Reset() noexcept;

    AudioSessionMode mode_ = AudioSessionMode::kRecording;
    std::string path_;
    std::function<Status()> start_fn_;
    std::function<Status()> stop_fn_;
    std::function<bool()> active_fn_;
};

}  // namespace audio_sdk
