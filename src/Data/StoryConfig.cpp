#include "Data/StoryConfig.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

namespace {
std::string PathText(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
bool ValidPhase(const std::string& phase) {
    return phase == "Prologue" || phase == "Story0" || phase == "Story1" ||
        phase == "Story2" || phase == "Story3" || phase == "Ending";
}
}

bool LoadStoryConfig(const std::filesystem::path& path,
    StoryConfig& config, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "Could not open story config: " + PathText(path);
        return false;
    }
    StoryConfig parsed;
    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        const auto comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        std::istringstream stream(line);
        std::string command;
        if (!(stream >> command)) continue;
        bool valid = false;
        if (command == "page") {
            StoryPage page;
            int autoScroll = -1;
            int mask = -1;
            valid = static_cast<bool>(stream >> page.phase >> page.image >> page.width >>
                page.height >> page.dialogueCount >> page.firstY >> page.stepY >> autoScroll >> mask);
            valid = valid && ValidPhase(page.phase) && !page.image.empty() &&
                std::filesystem::path(page.image).parent_path().empty() &&
                !std::filesystem::path(page.image).is_absolute() &&
                page.width > 0.0f && page.height > 0.0f && page.dialogueCount >= 0 &&
                (autoScroll == 0 || autoScroll == 1) && (mask == 0 || mask == 1);
            page.autoScroll = autoScroll != 0;
            page.mask = mask != 0;
            if (valid) parsed.pages.push_back(page);
        } else if (command == "timing") {
            valid = static_cast<bool>(stream >> parsed.curtainSeconds >> parsed.slideSpeed >>
                parsed.exitSpeed >> parsed.skipSpeed >> parsed.prologueSpeed >> parsed.prologueFastSpeed);
            valid = valid && parsed.curtainSeconds > 0.0f && parsed.slideSpeed > 0.0f &&
                parsed.exitSpeed > 0.0f && parsed.skipSpeed > 0.0f &&
                parsed.prologueSpeed > 0.0f && parsed.prologueFastSpeed > 0.0f;
        }
        std::string extra;
        if (!valid || (stream >> extra)) {
            error = PathText(path) + ":" + std::to_string(lineNumber) +
                ": invalid story entry";
            return false;
        }
    }
    for (const std::string phase : {"Prologue", "Story0", "Story1", "Story2", "Story3", "Ending"}) {
        if (std::none_of(parsed.pages.begin(), parsed.pages.end(),
                [&](const StoryPage& page) { return page.phase == phase; })) {
            error = PathText(path) + ": missing page for " + phase;
            return false;
        }
    }
    config = std::move(parsed);
    return true;
}
