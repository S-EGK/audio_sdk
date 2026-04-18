#include "audio_sdk/config.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <utility>

namespace audio_sdk {

namespace {

std::string ReadFile(const std::string& path) {
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string EscapeJson(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        if (ch == '\\' || ch == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

bool ExtractString(const std::string& text, const std::string& key, std::string& value) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) {
        return false;
    }
    value = match[1].str();
    return true;
}

bool ExtractInt(const std::string& text, const std::string& key, int& value) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) {
        return false;
    }
    value = std::stoi(match[1].str());
    return true;
}

bool ExtractBool(const std::string& text, const std::string& key, bool& value) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) {
        return false;
    }
    value = match[1].str() == "true";
    return true;
}

}  // namespace

Status Status::Ok() {
    return Status{true, {}};
}

Status Status::Error(std::string message_text) {
    return Status{false, std::move(message_text)};
}

Status LoadConfig(const std::string& path, AudioConfig& config) {
    if (!std::filesystem::exists(path)) {
        return Status::Error("Config file does not exist: " + path);
    }

    const std::string text = ReadFile(path);
    if (text.empty()) {
        return Status::Error("Config file is empty: " + path);
    }

    ExtractString(text, "backend", config.backend);

    ExtractString(text, "input_preferred_id", config.input.preferred_id);
    ExtractString(text, "input_preferred_name", config.input.preferred_name);
    ExtractString(text, "input_fallback_id", config.input.fallback_id);
    ExtractString(text, "input_fallback_name", config.input.fallback_name);

    ExtractString(text, "output_preferred_id", config.output.preferred_id);
    ExtractString(text, "output_preferred_name", config.output.preferred_name);
    ExtractString(text, "output_fallback_id", config.output.fallback_id);
    ExtractString(text, "output_fallback_name", config.output.fallback_name);

    ExtractString(text, "recording_output_path", config.recording.output_path);
    ExtractBool(text, "recording_delete_after_playback", config.recording.delete_after_playback);

    ExtractInt(text, "stream_sample_rate", config.stream.sample_rate);
    ExtractInt(text, "stream_channels", config.stream.channels);
    ExtractBool(text, "stream_strict_sync", config.stream.strict_sync);

    return Status::Ok();
}

Status SaveConfig(const std::string& path, const AudioConfig& config) {
    const std::filesystem::path file_path(path);
    if (!file_path.parent_path().empty()) {
        std::filesystem::create_directories(file_path.parent_path());
    }

    std::ofstream output(path);
    if (!output) {
        return Status::Error("Failed to open config file for writing: " + path);
    }

    output
        << "{\n"
        << "  \"backend\": \"" << EscapeJson(config.backend) << "\",\n"
        << "  \"input_preferred_id\": \"" << EscapeJson(config.input.preferred_id) << "\",\n"
        << "  \"input_preferred_name\": \"" << EscapeJson(config.input.preferred_name) << "\",\n"
        << "  \"input_fallback_id\": \"" << EscapeJson(config.input.fallback_id) << "\",\n"
        << "  \"input_fallback_name\": \"" << EscapeJson(config.input.fallback_name) << "\",\n"
        << "  \"output_preferred_id\": \"" << EscapeJson(config.output.preferred_id) << "\",\n"
        << "  \"output_preferred_name\": \"" << EscapeJson(config.output.preferred_name) << "\",\n"
        << "  \"output_fallback_id\": \"" << EscapeJson(config.output.fallback_id) << "\",\n"
        << "  \"output_fallback_name\": \"" << EscapeJson(config.output.fallback_name) << "\",\n"
        << "  \"recording_output_path\": \"" << EscapeJson(config.recording.output_path) << "\",\n"
        << "  \"recording_delete_after_playback\": "
        << (config.recording.delete_after_playback ? "true" : "false") << ",\n"
        << "  \"stream_sample_rate\": " << config.stream.sample_rate << ",\n"
        << "  \"stream_channels\": " << config.stream.channels << ",\n"
        << "  \"stream_strict_sync\": " << (config.stream.strict_sync ? "true" : "false") << "\n"
        << "}\n";

    return Status::Ok();
}

}  // namespace audio_sdk
