#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct StoryPage {
    std::string phase;
    std::string image;
    float width = 0.0f;
    float height = 0.0f;
    int dialogueCount = 0;
    float firstY = 0.0f;
    float stepY = 0.0f;
    bool autoScroll = false;
    bool mask = false;
};

struct StoryConfig {
    float curtainSeconds = 0.55f;
    float slideSpeed = 300.0f;
    float exitSpeed = 1500.0f;
    float skipSpeed = 1800.0f;
    float prologueSpeed = 180.0f;
    float prologueFastSpeed = 1620.0f;
    std::vector<StoryPage> pages;
};

bool LoadStoryConfig(const std::filesystem::path& path,
    StoryConfig& config, std::string& error);
