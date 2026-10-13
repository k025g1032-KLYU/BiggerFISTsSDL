#pragma once

#include "Input/GameInput.h"

#include <filesystem>
#include <istream>
#include <string>
#include <vector>

// One script tick corresponds to one fixed 1/60-second simulation update.
class PresentationReplay {
public:
    bool Load(const std::filesystem::path& path, int frameCount, std::string& error);
    bool Parse(std::istream& stream, const std::string& source,
        int frameCount, std::string& error);
    const GameInput& GetFrame(int tick) const;

private:
    std::vector<GameInput> frames_;
};
