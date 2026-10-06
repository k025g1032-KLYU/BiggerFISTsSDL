#pragma once

struct ApplicationOptions {
    bool smokeTest = false;
    bool scenePreview = false;
};

class Application {
public:
    int Run(const ApplicationOptions& options = {});
};
