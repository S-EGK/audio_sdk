#include <cassert>
#include <filesystem>
#include <iostream>
#include <sstream>

#include <unistd.h>

#include "audio_sdk/audio_manager.h"

int main() {
    namespace fs = std::filesystem;

    std::ostringstream temp_name;
    temp_name << "audio_sdk_smoke_" << getpid();

    const fs::path temp_root = fs::temp_directory_path() / temp_name.str();
    const fs::path config_path = temp_root / "config.json";
    fs::create_directories(temp_root);

    audio_sdk::AudioManager manager;

    const audio_sdk::Status save_status = manager.SaveConfigToFile(config_path.string());
    assert(save_status.ok);
    assert(fs::exists(config_path));

    audio_sdk::AudioManager loaded_manager;
    const audio_sdk::Status load_status = loaded_manager.LoadConfigFromFile(config_path.string());
    assert(load_status.ok);
    assert(loaded_manager.config().backend == "pipewire");
    assert(loaded_manager.config().stream.strict_sync);

    const auto devices = loaded_manager.EnumerateDevices();
    for (const auto& device : devices) {
        assert(!device.id.empty());
        assert(!device.name.empty());
    }

    fs::remove_all(temp_root);

    std::cout << "audio_sdk smoke tests passed\n";
    return 0;
}
