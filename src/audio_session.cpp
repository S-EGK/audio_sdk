#include "audio_sdk/audio_session.h"

#include <utility>

namespace audio_sdk {

AudioSession::AudioSession(
    AudioSessionMode mode,
    std::string path,
    std::function<Status()> start_fn,
    std::function<Status()> stop_fn,
    std::function<bool()> active_fn)
    : mode_(mode),
      path_(std::move(path)),
      start_fn_(std::move(start_fn)),
      stop_fn_(std::move(stop_fn)),
      active_fn_(std::move(active_fn)) {}

AudioSession::~AudioSession() {
    if (active()) {
        static_cast<void>(Stop());
    }
}

AudioSession::AudioSession(AudioSession&& other) noexcept
    : mode_(other.mode_),
      path_(std::move(other.path_)),
      start_fn_(std::move(other.start_fn_)),
      stop_fn_(std::move(other.stop_fn_)),
      active_fn_(std::move(other.active_fn_)) {
    other.Reset();
}

AudioSession& AudioSession::operator=(AudioSession&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (active()) {
        static_cast<void>(Stop());
    }

    mode_ = other.mode_;
    path_ = std::move(other.path_);
    start_fn_ = std::move(other.start_fn_);
    stop_fn_ = std::move(other.stop_fn_);
    active_fn_ = std::move(other.active_fn_);
    other.Reset();
    return *this;
}

Status AudioSession::Start() {
    if (!start_fn_) {
        return Status::Error("Session is not startable");
    }
    return start_fn_();
}

Status AudioSession::Stop() {
    if (!stop_fn_) {
        return Status::Error("Session is not stoppable");
    }
    return stop_fn_();
}

bool AudioSession::active() const {
    return active_fn_ ? active_fn_() : false;
}

void AudioSession::Reset() noexcept {
    start_fn_ = nullptr;
    stop_fn_ = nullptr;
    active_fn_ = nullptr;
    path_.clear();
}

}  // namespace audio_sdk
