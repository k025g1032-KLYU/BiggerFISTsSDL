#include "Data/SceneConfig.h"

#include <array>
#include <cmath>
#include <fstream>
#include <locale>
#include <span>
#include <sstream>
#include <utility>

namespace {
constexpr unsigned IdField = 1;
constexpr unsigned PositionField = 2;
constexpr unsigned ScaleField = 4;
constexpr unsigned RotationField = 8;
constexpr unsigned SpinField = 16;
constexpr unsigned RequiredFields = IdField | PositionField | ScaleField | RotationField | SpinField;

bool IsIdentifier(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    const auto isLetter = [](char character) {
        return (character >= 'A' && character <= 'Z') ||
            (character >= 'a' && character <= 'z') || character == '_';
    };
    if (!isLetter(value.front())) {
        return false;
    }
    for (const char character : value) {
        if (!isLetter(character) && !(character >= '0' && character <= '9') && character != '-') {
            return false;
        }
    }
    return true;
}

std::string TrimWhitespace(std::string text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

std::string PathToUtf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
}

std::filesystem::path PathFromUtf8(const std::string& text) {
    const auto* bytes = reinterpret_cast<const char8_t*>(text.data());
    return std::filesystem::path(std::u8string(bytes, bytes + text.size()));
}

bool ParseFloats(const std::string& text, std::span<float> values) {
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    for (float& value : values) {
        if (!(input >> value) || !std::isfinite(value)) {
            return false;
        }
    }
    input >> std::ws;
    return input.eof();
}

bool IsRelativeObjPath(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute() || path.has_root_path() || path.extension() != ".obj") {
        return false;
    }
    for (const auto& part : path) {
        if (part == "." || part == "..") {
            return false;
        }
    }
    return true;
}
}

bool LoadSceneConfig(const std::filesystem::path& configPath, SceneConfig& result, std::string& error) {
    result = {};
    error.clear();
    std::ifstream file(configPath, std::ios::binary);
    if (!file) {
        error = "Could not open scene config: " + PathToUtf8(configPath);
        return false;
    }

    SceneConfig loaded;
    bool hasCamera = false;
    unsigned modelFields = 0;
    std::size_t modelLine = 0;
    std::string line;
    std::size_t lineNumber = 0;
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
        const std::string location = PathToUtf8(configPath) + ":" + std::to_string(lineNumber) + ": ";

        if (key == "camera") {
            std::array<float, 3> values{};
            if (hasCamera || !ParseFloats(value, values) ||
                (values[0] == 0.0f && values[1] == 0.0f && values[2] == 0.0f)) {
                error = location + "expected one 'camera <x> <y> <z>' away from the origin";
                return false;
            }
            loaded.cameraPosition = { values[0], values[1], values[2] };
            hasCamera = true;
            continue;
        }

        if (key == "model") {
            if (!loaded.models.empty() && modelFields != RequiredFields) {
                error = PathToUtf8(configPath) + ":" + std::to_string(modelLine) +
                    ": model requires id, position, scale, rotation and spin";
                return false;
            }
            std::filesystem::path relativePath;
            try {
                relativePath = PathFromUtf8(value);
            } catch (const std::filesystem::filesystem_error&) {
                error = location + "invalid UTF-8 model path";
                return false;
            }
            if (!IsRelativeObjPath(relativePath)) {
                error = location + "model must be a relative .obj path without . or ..";
                return false;
            }
            loaded.models.push_back({});
            loaded.models.back().relativeObjPath = std::move(relativePath);
            modelFields = 0;
            modelLine = lineNumber;
            continue;
        }

        if (loaded.models.empty()) {
            error = location + "expected camera or model before model properties";
            return false;
        }
        SceneModelConfig& model = loaded.models.back();
        if (key == "id" && !(modelFields & IdField)) {
            if (!IsIdentifier(value)) {
                error = location + "expected 'id <unique name>' using letters, digits, _ or -";
                return false;
            }
            for (std::size_t i = 0; i + 1 < loaded.models.size(); ++i) {
                if (loaded.models[i].id == value) {
                    error = location + "duplicate model id: " + value;
                    return false;
                }
            }
            model.id = value;
            modelFields |= IdField;
        } else if (key == "position" && !(modelFields & PositionField)) {
            std::array<float, 3> values{};
            if (!ParseFloats(value, values)) {
                error = location + "expected 'position <x> <y> <z>'";
                return false;
            }
            model.position = { values[0], values[1], values[2] };
            modelFields |= PositionField;
        } else if (key == "scale" && !(modelFields & ScaleField)) {
            std::array<float, 1> values{};
            if (!ParseFloats(value, values) || values[0] <= 0.0f) {
                error = location + "expected 'scale <positive number>'";
                return false;
            }
            model.scale = values[0];
            modelFields |= ScaleField;
        } else if (key == "rotation" && !(modelFields & RotationField)) {
            std::array<float, 2> values{};
            if (!ParseFloats(value, values)) {
                error = location + "expected 'rotation <x degrees> <y degrees>'";
                return false;
            }
            model.initialXDegrees = values[0];
            model.initialYDegrees = values[1];
            modelFields |= RotationField;
        } else if (key == "spin" && !(modelFields & SpinField)) {
            std::array<float, 2> values{};
            if (!ParseFloats(value, values)) {
                error = location + "expected 'spin <x degrees/second> <y degrees/second>'";
                return false;
            }
            model.rotationXDegreesPerSecond = values[0];
            model.rotationYDegreesPerSecond = values[1];
            modelFields |= SpinField;
        } else {
            error = location + "unknown or duplicate scene setting: " + key;
            return false;
        }
    }

    if (!file.eof() && file.fail()) {
        error = "Could not read scene config: " + PathToUtf8(configPath);
        return false;
    }
    if (!hasCamera) {
        error = "Scene config has no camera: " + PathToUtf8(configPath);
        return false;
    }
    if (loaded.models.empty()) {
        error = "Scene config has no models: " + PathToUtf8(configPath);
        return false;
    }
    if (modelFields != RequiredFields) {
        error = PathToUtf8(configPath) + ":" + std::to_string(modelLine) +
            ": model requires id, position, scale, rotation and spin";
        return false;
    }
    result = std::move(loaded);
    return true;
}
