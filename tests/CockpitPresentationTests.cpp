#include "Gameplay/Cockpit/CockpitPresentation.h"

#include <cmath>
#include <cstdio>

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
        cockpit.GetActivation().leftArm == 0.0f, "full startup begins closed");
    cockpit.Update(1.35f);
    pass &= Check(cockpit.GetVisibleLineCount() == 4 &&
        cockpit.GetActivation().leftArm > 0.0f &&
        !cockpit.GetHudVisibility().dodge, "midpoint activation and HUD");
    cockpit.Update(3.45f);
    pass &= Check(!cockpit.IsActive(), "full startup finishes at 4.8 seconds");
    cockpit.Start(CockpitMode::QuickStartup);
    cockpit.Update(0.4f);
    pass &= Check(cockpit.GetVisibleLineCount() == 3 &&
        cockpit.GetHudVisibility().dodge, "quick startup HUD order");
    cockpit.Update(1.1f);
    pass &= Check(!cockpit.IsActive(), "quick startup finishes at 1.5 seconds");
    cockpit.Start(CockpitMode::BossStartup);
    cockpit.Update(0.2f);
    pass &= Check(cockpit.GetHudVisibility().targets &&
        !cockpit.GetHudVisibility().leftArm, "boss startup HUD order");
    cockpit.Reset();
    cockpit.SetEmergency(true, 3.0f);
    pass &= Check(cockpit.GetMode() == CockpitMode::EmergencyReboot &&
        cockpit.GetActivation().body > 0.99f, "emergency starts online");
    cockpit.SetEmergency(true, 1.5f);
    pass &= Check(cockpit.GetActivation().body == 0.0f,
        "emergency midpoint offline");
    cockpit.SetEmergency(true, 0.5f);
    pass &= Check(cockpit.IsAllGreenVisible() &&
        cockpit.GetActivation().body > 0.0f, "emergency reboots");
    cockpit.SetEmergency(false, 0.0f);
    pass &= Check(!cockpit.IsActive(), "emergency clears");
    return pass ? 0 : 1;
}
