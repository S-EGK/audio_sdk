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

    audio_sdk::AudioConfig config;
    config.input.preferred_name = "Unit Test Mic";
    config.input.fallback_name = "Backup Unit Test Mic";
    config.output.preferred_name = "Unit Test Speaker";
    config.output.fallback_name = "Backup Unit Test Speaker";

    audio_sdk::AudioManager manager(config);

    const audio_sdk::Status save_status = manager.SaveConfigToFile(config_path.string());
    assert(save_status.ok);
    assert(fs::exists(config_path));

    audio_sdk::AudioManager loaded_manager;
    const audio_sdk::Status load_status = loaded_manager.LoadConfigFromFile(config_path.string());
    assert(load_status.ok);
    assert(loaded_manager.config().backend == "pipewire");
    assert(loaded_manager.config().input.preferred_name == "Unit Test Mic");
    assert(loaded_manager.config().input.fallback_name == "Backup Unit Test Mic");
    assert(loaded_manager.config().output.preferred_name == "Unit Test Speaker");
    assert(loaded_manager.config().output.fallback_name == "Backup Unit Test Speaker");
    assert(loaded_manager.config().stream.strict_sync);

    assert(manager.SelectInputDevice("manual-input-id"));
    assert(manager.config().input.preferred_id == "manual-input-id");
    assert(manager.config().input.preferred_name.empty());

    assert(manager.SelectOutputDevice("manual-output-id"));
    assert(manager.config().output.preferred_id == "manual-output-id");
    assert(manager.config().output.preferred_name.empty());

    const auto devices = loaded_manager.EnumerateDevices();
    for (const auto& device : devices) {
        assert(!device.id.empty());
        assert(!device.name.empty());
    }

    fs::remove_all(temp_root);

    std::cout << "audio_sdk smoke tests passed\n";
    return 0;
}
