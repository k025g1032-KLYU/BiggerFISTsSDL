#include "Data/ModelConfig.h"

#include <fstream>
#include <sstream>
#include <utility>

namespace {
std::string TrimWhitespace(std::string text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

std::filesystem::path PathFromUtf8(const std::string& text) {
    const auto* bytes = reinterpret_cast<const char8_t*>(text.data());
    return std::filesystem::path(std::u8string(bytes, bytes + text.size()));
}

std::string PathToUtf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
}
}

bool LoadModelConfig(const std::filesystem::path& configPath, ModelConfig& result, std::string& error) {
    result = {};
    error.clear();

    std::ifstream file(configPath, std::ios::binary);
    if (!file) {
        error = "Could not open model config: " + PathToUtf8(configPath);
        return false;
    }

    std::string line;
    size_t lineNumber = 0;
    bool foundModel = false;
    std::filesystem::path selectedPath;
    while (std::getline(file, line)) {
        ++lineNumber;
        if (lineNumber == 1 && line.starts_with("\xEF\xBB\xBF")) {
            line.erase(0, 3);
        }
        line = TrimWhitespace(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream input(line);
        std::string key;
        input >> key;
        std::string value;
        std::getline(input, value);
        value = TrimWhitespace(value);
        if (key != "model" || value.empty() || foundModel) {
            error = PathToUtf8(configPath) + ":" + std::to_string(lineNumber) +
                ": expected one 'model <relative OBJ path>' entry";
            return false;
        }

        std::filesystem::path relativePath;
        try {
            relativePath = PathFromUtf8(value);
        } catch (const std::filesystem::filesystem_error&) {
            error = PathToUtf8(configPath) + ":" + std::to_string(lineNumber) +
                ": invalid UTF-8 model path";
            return false;
        }
        if (relativePath.is_absolute() || relativePath.has_root_path() ||
            relativePath.extension() != ".obj") {
            error = PathToUtf8(configPath) + ":" + std::to_string(lineNumber) +
                ": model must be a relative .obj path";
            return false;
        }
        for (const auto& part : relativePath) {
            if (part == "." || part == "..") {
                error = PathToUtf8(configPath) + ":" + std::to_string(lineNumber) +
                    ": model path cannot contain . or ..";
                return false;
            }
        }

        selectedPath = relativePath;
        foundModel = true;
    }

    if (!file.eof() && file.fail()) {
        error = "Could not read model config: " + PathToUtf8(configPath);
        result = {};
        return false;
    }
    if (!foundModel) {
        error = "Model config has no model entry: " + PathToUtf8(configPath);
        return false;
    }
    result.relativeObjPath = std::move(selectedPath);
    return true;
}
