#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>

#include <utility>

#include "audio_sdk/audio_manager.h"

namespace py = pybind11;

PYBIND11_MODULE(_audio_sdk, module) {
    module.doc() = "audio_sdk Python bindings";

    py::enum_<audio_sdk::DeviceDirection>(module, "DeviceDirection")
        .value("INPUT", audio_sdk::DeviceDirection::kInput)
        .value("OUTPUT", audio_sdk::DeviceDirection::kOutput);

    py::enum_<audio_sdk::AudioSessionMode>(module, "AudioSessionMode")
        .value("RECORDING", audio_sdk::AudioSessionMode::kRecording)
        .value("PLAYBACK", audio_sdk::AudioSessionMode::kPlayback);

    py::enum_<audio_sdk::EventType>(module, "EventType")
        .value("DEVICE_ADDED", audio_sdk::EventType::kDeviceAdded)
        .value("DEVICE_REMOVED", audio_sdk::EventType::kDeviceRemoved)
        .value("DEVICE_AVAILABLE_CHANGED", audio_sdk::EventType::kDeviceAvailableChanged)
        .value("DEFAULT_INPUT_CHANGED", audio_sdk::EventType::kDefaultInputChanged)
        .value("DEFAULT_OUTPUT_CHANGED", audio_sdk::EventType::kDefaultOutputChanged)
        .value("STREAM_STARTED", audio_sdk::EventType::kStreamStarted)
        .value("STREAM_STOPPED", audio_sdk::EventType::kStreamStopped)
        .value("STREAM_ERROR", audio_sdk::EventType::kStreamError)
        .value("STREAM_FALLBACK_APPLIED", audio_sdk::EventType::kStreamFallbackApplied)
        .value("UNDERRUN", audio_sdk::EventType::kUnderrun)
        .value("OVERRUN", audio_sdk::EventType::kOverrun)
        .value("ROUTE_CHANGED", audio_sdk::EventType::kRouteChanged)
        .value("RECORDING_SAVED", audio_sdk::EventType::kRecordingSaved)
        .value("RECORDING_DELETED", audio_sdk::EventType::kRecordingDeleted);

    py::class_<audio_sdk::DeviceDescriptor>(module, "DeviceDescriptor")
        .def(py::init<>())
        .def_readwrite("id", &audio_sdk::DeviceDescriptor::id)
        .def_readwrite("name", &audio_sdk::DeviceDescriptor::name)
        .def_readwrite("direction", &audio_sdk::DeviceDescriptor::direction)
        .def_readwrite("is_default", &audio_sdk::DeviceDescriptor::is_default)
        .def_readwrite("available", &audio_sdk::DeviceDescriptor::available);

    py::class_<audio_sdk::Event>(module, "Event")
        .def(py::init<>())
        .def_readwrite("type", &audio_sdk::Event::type)
        .def_readwrite("device_id", &audio_sdk::Event::device_id)
        .def_readwrite("details", &audio_sdk::Event::details);

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
        .def_readonly("message", &audio_sdk::Status::message)
        .def("__bool__", [](const audio_sdk::Status& status) {
            return status.ok;
        })
        .def("__repr__", [](const audio_sdk::Status& status) {
            return status.ok ? "Status(ok=True)" : "Status(ok=False, message='" + status.message + "')";
        });

    py::class_<audio_sdk::AudioSession, std::unique_ptr<audio_sdk::AudioSession>>(module, "AudioSession")
        .def("start", &audio_sdk::AudioSession::Start)
        .def("stop", &audio_sdk::AudioSession::Stop)
        .def("active", &audio_sdk::AudioSession::active)
        .def_property_readonly("mode", &audio_sdk::AudioSession::mode)
        .def_property_readonly("path", &audio_sdk::AudioSession::path);

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
        .def("delete_recording", &audio_sdk::AudioManager::DeleteRecording)
        .def("create_recording_session", &audio_sdk::AudioManager::CreateRecordingSession)
        .def("create_playback_session", &audio_sdk::AudioManager::CreatePlaybackSession)
        .def("set_event_callback", [](audio_sdk::AudioManager& manager, py::object callback) {
            if (callback.is_none()) {
                manager.SetEventCallback({});
                return;
            }

            py::function event_callback = py::reinterpret_borrow<py::function>(callback);
            manager.SetEventCallback([event_callback = std::move(event_callback)](const audio_sdk::Event& event) {
                py::gil_scoped_acquire acquire;
                event_callback(event);
            });
        })
        .def_property_readonly(
            "config",
            [](const audio_sdk::AudioManager& manager) -> const audio_sdk::AudioConfig& {
                return manager.config();
            },
            py::return_value_policy::reference_internal);
}
