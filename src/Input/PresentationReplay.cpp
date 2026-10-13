#include "Input/PresentationReplay.h"

#include <cmath>
#include <array>
#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_set>

namespace {
bool SetHeld(GameInput& input, const std::string& field, const std::string& value) {
    if (field == "moveX" || field == "moveY") {
        std::istringstream number(value);
        float axis = 0.0f;
        char extra = 0;
        if (!(number >> axis) || (number >> extra) || !std::isfinite(axis) ||
            axis < -1.0f || axis > 1.0f)
            return false;
        (field == "moveX" ? input.moveX : input.moveY) = axis;
        return true;
    }
    if (value != "0" && value != "1")
        return false;
    const bool held = value == "1";
    if (field == "leftPunch") input.leftPunchKeyDown = held;
    else if (field == "rightPunch") input.rightPunchKeyDown = held;
    else if (field == "leftRocket") input.leftMouseDown = held;
    else if (field == "rightRocket") input.rightMouseDown = held;
    else if (field == "shift") input.shiftDown = held;
    else return false;
    return true;
}

bool Press(GameInput& input, const std::string& action) {
    ActionState* state = nullptr;
    if (action == "start") state = &input.battleStart;
    else if (action == "confirm") state = &input.confirm;
    else if (action == "skip") state = &input.skip;
    else if (action == "skipPhase") state = &input.skipPhase;
    else if (action == "switchView") state = &input.switchView;
    else if (action == "ThirdPerson") state = &input.toggleThirdPerson;
    else if (action == "CycleCockpitModel") state = &input.cycleCockpitModel;
    else if (action == "ToggleData") state = &input.toggleCombatData;
    else if (action == "dodge") state = &input.dodge;
    else if (action == "reset") state = &input.resetTarget;
    else if (action == "projectileHit") state = &input.testProjectileHit;
    else if (action == "chargeHit") state = &input.testChargeHit;
    else if (action == "burstHit") state = &input.testBurstHit;
    else return false;
    state->down = true;
    state->pressed = true;
    return true;
}

bool SetPractice(GameInput& input, std::string_view name) {
    if (name == "ThirdPerson" || name == "CycleCockpitModel" ||
        name == "ToggleData") {
        return Press(input, std::string(name));
    }
    constexpr std::array commands{
        std::pair{"MissileUnit", PracticeCommand::MissileUnit},
        std::pair{"Bomber", PracticeCommand::Bomber},
        std::pair{"ChargeUnit", PracticeCommand::ChargeUnit},
        std::pair{"MissileUnits", PracticeCommand::MissileUnits},
        std::pair{"Bombers", PracticeCommand::Bombers},
        std::pair{"ChargeUnits", PracticeCommand::ChargeUnits},
        std::pair{"MixedEnemies", PracticeCommand::MixedEnemies},
        std::pair{"BossSpawn", PracticeCommand::BossSpawn},
        std::pair{"BossCycleNeedles", PracticeCommand::BossCycleNeedles},
        std::pair{"BossConeCharge", PracticeCommand::BossConeCharge},
        std::pair{"BossBoxer", PracticeCommand::BossBoxer},
        std::pair{"BossNeedleCombo", PracticeCommand::BossNeedleCombo},
        std::pair{"BossFullBarrage", PracticeCommand::BossFullBarrage},
        std::pair{"BossSpreadBurst", PracticeCommand::BossSpreadBurst},
        std::pair{"SingleMissile", PracticeCommand::SingleMissile},
        std::pair{"MissileLine", PracticeCommand::MissileLine},
        std::pair{"FloatingBombs", PracticeCommand::FloatingBombs},
        std::pair{"MixedProjectiles", PracticeCommand::MixedProjectiles},
        std::pair{"FullStartup", PracticeCommand::FullStartup},
        std::pair{"QuickStartup", PracticeCommand::QuickStartup},
        std::pair{"BossStartup", PracticeCommand::BossStartup},
        std::pair{"SystemOffline", PracticeCommand::SystemOffline}};
    for (const auto& [key, command] : commands) {
        if (name == key) {
            input.practiceCommand = command;
            return true;
        }
    }
    return false;
}

bool ReadFloat(const std::string& value, float& result) {
    std::istringstream number(value);
    char extra = 0;
    return (number >> result) && !(number >> extra) && std::isfinite(result) &&
        std::abs(result) <= 10000.0f;
}
}

bool PresentationReplay::Load(const std::filesystem::path& path,
    int frameCount, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = path.string() + ": cannot open input replay";
        return false;
    }
    return Parse(file, path.string(), frameCount, error);
}

bool PresentationReplay::Parse(std::istream& stream, const std::string& source,
    int frameCount, std::string& error) {
    frames_.clear();
    if (frameCount <= 0 || frameCount > 3600) {
        error = source + ": frame count must be 1..3600";
        return false;
    }
    struct Command {
        std::string verb;
        std::string field;
        std::string value;
        std::string secondValue;
    };
    std::vector<std::vector<Command>> commands(frameCount);
    std::unordered_set<std::string> seen;
    std::string line;
    int lineNumber = 0;
    while (std::getline(stream, line)) {
        ++lineNumber;
        const size_t comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        std::istringstream words(line);
        std::string verb;
        if (!(words >> verb)) continue;
        int tick = -1;
        Command command{};
        command.verb = verb;
        std::string extra;
        if (!(words >> tick) || tick < 0 || tick >= frameCount) {
            error = source + ":" + std::to_string(lineNumber) +
                ": tick must be within 0.." + std::to_string(frameCount - 1);
            return false;
        }
        if (!(words >> command.field) ||
            (verb == "set" && (!(words >> command.value) || (words >> extra))) ||
            ((verb == "press" || verb == "practice") && (words >> extra)) ||
            (verb == "mouse" && (!(words >> command.value >> command.secondValue) ||
                (words >> extra))) ||
            (verb != "set" && verb != "press" && verb != "practice" &&
                verb != "mouse")) {
            error = source + ":" + std::to_string(lineNumber) + ": invalid replay command";
            return false;
        }
        GameInput validation{};
        float x = 0.0f;
        float y = 0.0f;
        const bool valid = verb == "set"
            ? SetHeld(validation, command.field, command.value)
            : verb == "press" ? Press(validation, command.field)
            : verb == "practice" ? SetPractice(validation, command.field)
            : command.field == "look" && ReadFloat(command.value, x) &&
                ReadFloat(command.secondValue, y);
        if (!valid || !seen.insert(std::to_string(tick) + ":" +
                verb + ":" + command.field).second) {
            error = source + ":" + std::to_string(lineNumber) +
                ": invalid or duplicate replay input";
            return false;
        }
        commands[tick].push_back(std::move(command));
    }
    if (stream.bad()) {
        error = source + ": error reading input replay";
        return false;
    }
    GameInput held{};
    frames_.reserve(frameCount);
    for (int tick = 0; tick < frameCount; ++tick) {
        GameInput frame{};
        for (const Command& command : commands[tick]) {
            if (command.verb == "set")
                SetHeld(held, command.field, command.value);
        }
        frame = held;
        for (const Command& command : commands[tick]) {
            if (command.verb == "press") Press(frame, command.field);
            else if (command.verb == "practice")
                SetPractice(frame, command.field);
            else if (command.verb == "mouse") {
                ReadFloat(command.value, frame.lookX);
                ReadFloat(command.secondValue, frame.lookY);
            }
        }
        frames_.push_back(frame);
    }
    error.clear();
    return true;
}

const GameInput& PresentationReplay::GetFrame(int tick) const {
    return frames_.at(static_cast<size_t>(tick));
}
