#include "Input/PresentationReplay.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {
bool Check(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}
}

int main() {
    PresentationReplay replay;
    std::string error;
    std::istringstream script(
        "# Fixed 1/60-second ticks\n"
        "set 2 leftRocket 1\n"
        "set 4 leftRocket 0\n"
        "press 4 projectileHit\n"
        "mouse 4 look 5 -3\n"
        "set 1 moveX -0.5\n"
        "press 7 reset\n"
        "practice 8 BossSpawn\n");
    bool success = Check(replay.Parse(script, "replay.txt", 9, error),
        "valid replay must parse");
    if (!success) return 1;
    success &= Check(replay.GetFrame(0).moveX == 0.0f &&
        replay.GetFrame(1).moveX == -0.5f &&
        replay.GetFrame(8).moveX == -0.5f,
        "axis setting must persist from its tick");
    success &= Check(!replay.GetFrame(1).leftMouseDown &&
        replay.GetFrame(2).leftMouseDown &&
        replay.GetFrame(3).leftMouseDown &&
        !replay.GetFrame(4).leftMouseDown,
        "rocket hold must end on the specified tick");
    success &= Check(replay.GetFrame(4).testProjectileHit.pressed &&
        !replay.GetFrame(5).testProjectileHit.pressed &&
        replay.GetFrame(7).resetTarget.pressed &&
        !replay.GetFrame(8).resetTarget.pressed,
        "one-shot actions must not repeat");
    success &= Check(replay.GetFrame(4).lookX == 5.0f &&
        replay.GetFrame(4).lookY == -3.0f &&
        replay.GetFrame(5).lookX == 0.0f,
        "mouse movement must last only one tick");
    success &= Check(replay.GetFrame(8).practiceCommand ==
        PracticeCommand::BossSpawn && replay.GetFrame(7).practiceCommand ==
        PracticeCommand::None, "practice command must last only one tick");

    std::istringstream legacyChecks(
        "practice 1 ThirdPerson\npractice 2 CycleCockpitModel\n"
        "practice 3 ToggleData\npress 4 ThirdPerson\n");
    success &= Check(replay.Parse(legacyChecks, "legacy.txt", 5, error) &&
        replay.GetFrame(1).toggleThirdPerson.pressed &&
        replay.GetFrame(2).cycleCockpitModel.pressed &&
        replay.GetFrame(3).toggleCombatData.pressed &&
        replay.GetFrame(4).toggleThirdPerson.pressed &&
        replay.GetFrame(3).practiceCommand == PracticeCommand::None,
        "legacy practice checks must map to the shared inspection actions");

    std::istringstream invalid("set 0 leftRocket 1\nset 0 leftRocket 0\n");
    success &= Check(!replay.Parse(invalid, "bad.txt", 9, error) &&
        error.find("bad.txt:2") != std::string::npos,
        "duplicate assignment must report file and line");
    std::istringstream outOfRange("press 9 dodge\n");
    success &= Check(!replay.Parse(outOfRange, "range.txt", 9, error) &&
        error.find("range.txt:1") != std::string::npos,
        "tick outside capture length must fail");
    std::istringstream badNumber("set 1 moveY nan\n");
    success &= Check(!replay.Parse(badNumber, "nan.txt", 9, error),
        "non-finite axis must fail");
    std::istringstream badPractice("practice 2 MissingCommand\n");
    success &= Check(!replay.Parse(badPractice, "practice.txt", 9, error) &&
        error.find("practice.txt:1") != std::string::npos,
        "unknown practice command must report file and line");
    return success ? 0 : 1;
}
