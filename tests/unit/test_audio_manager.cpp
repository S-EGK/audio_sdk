#include <cassert>
#include <filesystem>
#include <iostream>

#include "audio_sdk/audio_manager.h"

int main() {
    namespace fs = std::filesystem;

    const fs::path temp_root = fs::temp_directory_path() / "audio_sdk_smoke";
    const fs::path config_path = temp_root / "config.json";
    const fs::path wav_path = temp_root / "smoke.wav";

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

    const audio_sdk::Status start_status = manager.StartRecording(wav_path.string());
    assert(start_status.ok);
    assert(fs::exists(wav_path));

    const audio_sdk::Status stop_status = manager.StopRecording();
    assert(stop_status.ok);

    const audio_sdk::Status play_status = manager.PlayRecording(wav_path.string());
    assert(play_status.ok);

    const audio_sdk::Status delete_status = manager.DeleteRecording(wav_path.string());
    assert(delete_status.ok);
    assert(!fs::exists(wav_path));

    fs::remove_all(temp_root);

    std::cout << "audio_sdk smoke tests passed\n";
    return 0;
}

