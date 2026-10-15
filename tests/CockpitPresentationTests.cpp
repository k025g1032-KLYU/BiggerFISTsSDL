#include "Gameplay/Cockpit/CockpitPresentation.h"

#include <cmath>
#include <cstdio>
#include <string_view>

namespace {
bool Check(bool result, const char* name) {
    if (!result) std::printf("FAIL: %s\n", name);
    return result;
}
}

int main() {
    bool pass = true;
    CockpitPresentation cockpit;
    cockpit.Start(CockpitMode::FullStartup);
    pass &= Check(cockpit.ConsumeStartCue() && !cockpit.ConsumeStartCue(),
        "startup cue only once");
    pass &= Check(cockpit.GetVisibleLineCount() == 1 &&
        cockpit.GetActivation().leftArm == 0.0f &&
        std::abs(cockpit.GetOverlayAlpha() - 0.75f) < 0.001f &&
        std::string_view(cockpit.GetTitle()) == "COCKPIT INITIALIZATION" &&
        std::string_view(cockpit.GetLine(0)) == "MAIN POWER ........ ONLINE",
        "full startup begins with the configured title, check and blackout");
    cockpit.Update(0.45f);
    pass &= Check(cockpit.GetVisibleLineCount() == 2 &&
        cockpit.GetHudVisibility().armor &&
        !cockpit.GetHudVisibility().leftArm,
        "full startup second check enables armor and shield");
    cockpit.Update(0.91f);
    pass &= Check(cockpit.GetVisibleLineCount() == 4 &&
        cockpit.GetHudVisibility().leftArm &&
        cockpit.GetHudVisibility().rightArm &&
        !cockpit.GetHudVisibility().dodge,
        "full startup third and fourth checks enable arms");
    pass &= Check(cockpit.GetVisibleLineCount() == 4 &&
        cockpit.GetActivation().leftArm > 0.0f &&
        !cockpit.GetHudVisibility().dodge, "midpoint activation and HUD");
    cockpit.Update(1.71f);
    pass &= Check(cockpit.GetVisibleLineCount() == 6 &&
        cockpit.GetHudVisibility().targets && cockpit.IsAllGreenVisible(),
        "full startup sixth check and all-green gate");
    cockpit.Update(1.74f);
    pass &= Check(!cockpit.IsActive(), "full startup finishes at 4.8 seconds");
    cockpit.Start(CockpitMode::QuickStartup);
    pass &= Check(cockpit.GetVisibleLineCount() == 1 &&
        cockpit.GetHudVisibility().base &&
        !cockpit.GetHudVisibility().leftArm &&
        std::abs(cockpit.GetOverlayAlpha() - 0.78f) < 0.001f,
        "quick startup begins with base and reduced blackout");
    cockpit.Update(0.4f);
    pass &= Check(cockpit.GetVisibleLineCount() == 3 &&
        cockpit.GetHudVisibility().dodge, "quick startup HUD order");
    cockpit.Update(1.1f);
    pass &= Check(!cockpit.IsActive(), "quick startup finishes at 1.5 seconds");
    cockpit.Start(CockpitMode::BossStartup);
    pass &= Check(cockpit.GetHudVisibility().targets &&
        !cockpit.GetHudVisibility().base &&
        std::string_view(cockpit.GetTitle()) == "BOSS ENGAGEMENT",
        "boss startup begins with target link");
    cockpit.Update(0.2f);
    pass &= Check(cockpit.GetHudVisibility().targets &&
        !cockpit.GetHudVisibility().leftArm, "boss startup HUD order");
    cockpit.Reset();
    cockpit.SetEmergency(true, 3.0f);
    pass &= Check(cockpit.GetMode() == CockpitMode::EmergencyReboot &&
        cockpit.GetActivation().body > 0.99f &&
        std::abs(cockpit.GetOverlayAlpha()) < 0.001f,
        "emergency starts online without a black overlay");
    cockpit.SetEmergency(true, 2.75f);
    pass &= Check(std::abs(cockpit.GetOverlayAlpha() - 0.325f) < 0.001f,
        "emergency blackout reaches half opacity at 0.25 seconds");
    cockpit.SetEmergency(true, 1.5f);
    pass &= Check(cockpit.GetActivation().body == 0.0f &&
        std::abs(cockpit.GetOverlayAlpha() - 0.65f) < 0.001f,
        "emergency midpoint remains visible at 0.65 opacity");
    cockpit.SetEmergency(true, 0.5f);
    pass &= Check(cockpit.IsAllGreenVisible() &&
        cockpit.GetActivation().body > 0.0f, "emergency reboots");
    cockpit.SetEmergency(false, 0.0f);
    pass &= Check(!cockpit.IsActive(), "emergency clears");
    return pass ? 0 : 1;
}
