#include "InputBindings.h"

#include <sstream>
#include <string>

InputBindings LoadInputBindings(const char* filePath) {
    InputBindings bindings;
    std::string path;

    if (filePath != nullptr) {
        path = filePath;
    }
    else {
        const char* basePath = SDL_GetBasePath();
        if (basePath == nullptr) {
            SDL_Log("SDL_GetBasePath failed: %s; using default input bindings", SDL_GetError());
            return bindings;
        }
        path = std::string(basePath) + "input.cfg";
    }

    size_t fileSize = 0;
    void* fileData = SDL_LoadFile(path.c_str(), &fileSize);
    if (fileData == nullptr) {
        SDL_Log("Could not load %s: %s; using default input bindings",
            path.c_str(), SDL_GetError());
        return bindings;
    }

    std::string text(static_cast<const char*>(fileData), fileSize);
    SDL_free(fileData);

    if (text.starts_with("\xEF\xBB\xBF")) {
        text.erase(0, 3);
    }

    std::istringstream fileInput(text);
    std::string line;
    int lineNumber = 0;

    while (std::getline(fileInput, line)) {
        ++lineNumber;
        std::istringstream lineInput(line);
        std::string action;
        if (!(lineInput >> action)) {
            continue;
        }

        SDL_Scancode* binding = nullptr;
        if (action == "moveLeft") {
            binding = &bindings.moveLeft;
        }
        else if (action == "moveRight") {
            binding = &bindings.moveRight;
        }
        else if (action == "moveUp") {
            binding = &bindings.moveUp;
        }
        else if (action == "moveDown") {
            binding = &bindings.moveDown;
        }
        else if (action == "changeBackground") {
            binding = &bindings.changeBackground;
        }
        else if (action == "quit") {
            binding = &bindings.quit;
        }
        else {
            SDL_Log("input.cfg line %d: unknown action '%s'; skipping line",
                lineNumber, action.c_str());
            continue;
        }

        std::string keyName;
        std::getline(lineInput >> std::ws, keyName);
        const size_t lastCharacter = keyName.find_last_not_of(" \t\r");
        if (lastCharacter == std::string::npos) {
            SDL_Log("input.cfg line %d: missing key for '%s'; skipping line",
                lineNumber, action.c_str());
            continue;
        }
        keyName.erase(lastCharacter + 1);

        const SDL_Scancode key = SDL_GetScancodeFromName(keyName.c_str());
        if (key == SDL_SCANCODE_UNKNOWN) {
            SDL_Log("input.cfg line %d: unknown key '%s'; skipping line",
                lineNumber, keyName.c_str());
            continue;
        }

        *binding = key;
    }

    SDL_Log("Loaded input bindings: left=%s, right=%s, up=%s, down=%s, changeBackground=%s, quit=%s",
        SDL_GetScancodeName(bindings.moveLeft),
        SDL_GetScancodeName(bindings.moveRight),
        SDL_GetScancodeName(bindings.moveUp),
        SDL_GetScancodeName(bindings.moveDown),
        SDL_GetScancodeName(bindings.changeBackground),
        SDL_GetScancodeName(bindings.quit));

    return bindings;
}
