#pragma once

#include <string>

namespace audio_sdk {

struct Status {
    bool ok = true;
    std::string message;

    static Status Ok();
    static Status Error(std::string message_text);

    explicit operator bool() const { return ok; }
};

}  // namespace audio_sdk

