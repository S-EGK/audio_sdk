#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "audio_sdk/audio_manager.h"

namespace py = pybind11;

PYBIND11_MODULE(_audio_sdk, module) {
    module.doc() = "audio_sdk Python bindings";

    py::class_<audio_sdk::StreamPolicy>(module, "StreamPolicy")
        .def(py::init<>())
        .def_readwrite("sample_rate", &audio_sdk::StreamPolicy::sample_rate)
        .def_readwrite("channels", &audio_sdk::StreamPolicy::channels)
        .def_readwrite("strict_sync", &audio_sdk::StreamPolicy::strict_sync);

    py::class_<audio_sdk::RecordingPolicy>(module, "RecordingPolicy")
        .def(py::init<>())
        .def_readwrite("output_path", &audio_sdk::RecordingPolicy::output_path)
        .def_readwrite("delete_after_playback", &audio_sdk::RecordingPolicy::delete_after_playback);

    py::class_<audio_sdk::DevicePolicy>(module, "DevicePolicy")
        .def(py::init<>())
        .def_readwrite("preferred_id", &audio_sdk::DevicePolicy::preferred_id)
        .def_readwrite("preferred_name", &audio_sdk::DevicePolicy::preferred_name)
        .def_readwrite("fallback_id", &audio_sdk::DevicePolicy::fallback_id)
        .def_readwrite("fallback_name", &audio_sdk::DevicePolicy::fallback_name);

    py::class_<audio_sdk::AudioConfig>(module, "AudioConfig")
        .def(py::init<>())
        .def_readwrite("backend", &audio_sdk::AudioConfig::backend)
        .def_readwrite("input", &audio_sdk::AudioConfig::input)
        .def_readwrite("output", &audio_sdk::AudioConfig::output)
        .def_readwrite("recording", &audio_sdk::AudioConfig::recording)
        .def_readwrite("stream", &audio_sdk::AudioConfig::stream);

    py::class_<audio_sdk::Status>(module, "Status")
        .def_readonly("ok", &audio_sdk::Status::ok)
        .def_readonly("message", &audio_sdk::Status::message);

    py::class_<audio_sdk::AudioManager>(module, "AudioManager")
        .def(py::init<audio_sdk::AudioConfig>(), py::arg("config") = audio_sdk::AudioConfig{})
        .def("load_config_from_file", &audio_sdk::AudioManager::LoadConfigFromFile)
        .def("save_config_to_file", &audio_sdk::AudioManager::SaveConfigToFile)
        .def("backend_name", &audio_sdk::AudioManager::BackendName)
        .def("enumerate_devices", &audio_sdk::AudioManager::EnumerateDevices)
        .def("select_input_device", &audio_sdk::AudioManager::SelectInputDevice)
        .def("select_output_device", &audio_sdk::AudioManager::SelectOutputDevice)
        .def("start_recording", &audio_sdk::AudioManager::StartRecording)
        .def("stop_recording", &audio_sdk::AudioManager::StopRecording)
        .def("play_recording", &audio_sdk::AudioManager::PlayRecording)
        .def("delete_recording", &audio_sdk::AudioManager::DeleteRecording);
}

