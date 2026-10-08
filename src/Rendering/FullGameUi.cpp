#include "Rendering/FullGameUi.h"

#include "Game/FirstPersonGame.h"
#include "Gameplay/Combat/CombatSession.h"
#include "Gameplay/Tutorial/TutorialSession.h"
#include "Rendering/UiRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {
constexpr UiColor kPanel{0.025f, 0.045f, 0.08f, 0.72f};
constexpr UiColor kGreen{0.35f, 1.0f, 0.75f, 1.0f};
constexpr UiColor kRed{1.0f, 0.35f, 0.3f, 1.0f};

void Bar(UiDrawList& list, float x, float y, float width, float ratio,
    UiColor color) {
    list.Rectangle(x, y, width, 12.0f, {0.07f, 0.11f, 0.13f, 0.8f});
    list.Rectangle(x, y, width * std::clamp(ratio, 0.0f, 1.0f), 12.0f, color);
}
void Segments(UiDrawList& list, float x, float y, int count, int filled,
    float width, UiColor color) {
    for (int index = 0; index < count; ++index) {
        list.Rectangle(x + index * (width + 3.0f), y, width, 10.0f,
            index < filled ? color : UiColor{0.14f, 0.2f, 0.23f, 0.8f});
    }
}
const char* ArmStatus(PunchArmState state) {
    switch (state) {
    case PunchArmState::Ready: return "READY";
    case PunchArmState::Charging: return "CHARGING";
    case PunchArmState::Charged: return "CHARGED";
    case PunchArmState::Punching: return "PUNCH";
    case PunchArmState::RocketFlying: return "ROCKET";
    case PunchArmState::RocketEvacuating: return "EVACUATE";
    case PunchArmState::Returning: return "RETURN";
    case PunchArmState::Recovery: return "RECOVERY";
    default: return "BUSY";
    }
}
void TargetFrames(UiDrawList& list, const FirstPersonGame& player) {
    const FirstPersonCamera& camera = player.GetCamera();
    const Vector3& eye = camera.GetPosition();
    const float yaw = camera.GetYaw(), pitch = camera.GetPitch();
    const float sineYaw = std::sin(yaw), cosineYaw = std::cos(yaw);
    const float sinePitch = std::sin(pitch), cosinePitch = std::cos(pitch);
    const Vector3 forward{sineYaw * cosinePitch, -sinePitch,
        cosineYaw * cosinePitch};
    const Vector3 right{cosineYaw, 0.0f, -sineYaw};
    const Vector3 up{sineYaw * sinePitch, cosinePitch,
        cosineYaw * sinePitch};
    constexpr float focalLength = 623.538f; // 720 / (2 * tan(60 degrees / 2))
    int sequence = 0;
    for (const FirstPersonTarget& target : player.GetTargets()) {
        if (target.hitPoints <= 0 || sequence >= 10) continue;
        const Vector3 relative{target.position.x - eye.x,
            target.position.y + 1.4f - eye.y, target.position.z - eye.z};
        const float depth = relative.x * forward.x + relative.y * forward.y +
            relative.z * forward.z;
        if (depth <= 0.1f) continue;
        const float x = 640.0f + focalLength *
            (relative.x * right.x + relative.z * right.z) / depth;
        const float y = 360.0f - focalLength *
            (relative.x * up.x + relative.y * up.y + relative.z * up.z) / depth;
        if (x < 32.0f || x > 1248.0f || y < 40.0f || y > 680.0f) continue;
        const UiColor tint = sequence % 2 == 0 ?
            UiColor{0.38f, 1.0f, 0.86f, 0.9f} :
            UiColor{0.55f, 0.85f, 1.0f, 0.9f};
        list.Rectangle(x - 32, y - 32, 64, 3, tint);
        list.Rectangle(x - 32, y + 29, 64, 3, tint);
        list.Rectangle(x - 32, y - 32, 3, 64, tint);
        list.Rectangle(x + 29, y - 32, 3, 64, tint);
        char label[24]{};
        std::snprintf(label, sizeof(label), "%c LOCK %d",
            sequence % 2 == 0 ? 'L' : 'R', sequence / 2 + 1);
        list.Text(label, x - 30, y - 57, 0.8f, tint);
        ++sequence;
    }
}
void DrawHud(UiDrawList& list, const FirstPersonGame& player,
    const CombatSession* battle, const TutorialSession* tutorial,
    float cockpitStartupRemaining) {
    bool base = true;
    bool armor = !tutorial || tutorial->IsCockpitConnected();
    bool shields = !tutorial || tutorial->GetStep() >= TutorialStep::Defense;
    bool leftArm = !tutorial || tutorial->GetStep() >= TutorialStep::Punch;
    bool rightArm = leftArm;
    bool dodge = !tutorial || tutorial->GetStep() >= TutorialStep::Dodge;
    bool targets = !tutorial || tutorial->GetStep() >= TutorialStep::RocketPunch;
    if (!tutorial && cockpitStartupRemaining > 0.0f) {
        const float elapsed = 4.8f - cockpitStartupRemaining;
        const int visibleLines = std::clamp(
            static_cast<int>(elapsed / 0.45f) + 1, 1, 6);
        base = visibleLines >= 1;
        armor = shields = visibleLines >= 2;
        leftArm = visibleLines >= 3;
        rightArm = visibleLines >= 4;
        dodge = visibleLines >= 5;
        targets = visibleLines >= 6;
    }
    char line[128]{};
    if (targets) TargetFrames(list, player);
    if (armor || shields) {
        list.Rectangle(31.0f, shields ? 579.0f : 620.0f, 326.0f,
            shields ? 116.0f : 73.0f, kPanel);
    }
    if (armor) {
        std::snprintf(line, sizeof(line), "ARMOR  %d / 5",
            player.GetVitality().GetHitPoints());
        list.Text(line, 44, 627, 1.0f, kGreen);
        Segments(list, 164, 635, 10,
            player.GetVitality().GetHitPoints() * 2, 13, kGreen);
    }
    if (shields) {
        list.Text("SHIELD", 44, 590, 1.0f, kGreen);
        Segments(list, 132, 595, 3,
            player.GetVitality().GetShieldLayers(), 34,
            {0.3f, 0.65f, 1.0f, 1.0f});
        Bar(list, 132, 615, 189,
            player.GetVitality().GetShieldRechargeRatio(),
            {0.3f, 0.65f, 1.0f, 1.0f});
    }
    if (base && (battle || tutorial)) {
        list.Rectangle(486, 8, 320, 92, kPanel);
        if (const BattleBoss* boss = battle ? battle->GetBoss() : nullptr) {
            std::snprintf(line, sizeof(line), "BOSS CORE %d / %d",
                boss->GetHitPoints(), boss->GetMaximumHitPoints());
            list.Text(line, 520, 22, 1.0f, kRed);
            Bar(list, 500, 70, 292,
                static_cast<float>(boss->GetHitPoints()) / boss->GetMaximumHitPoints(), kRed);
        } else if (battle) {
            std::snprintf(line, sizeof(line), "BASE HP %d  WAVE %d / %d",
                battle->GetBaseHitPoints(), battle->GetWaveIndex() + 1,
                battle->GetWaveCount());
            list.Text(line, 530, 22, 0.95f, kGreen);
            std::snprintf(line, sizeof(line), "ENEMIES %u   PROJECTILES %u",
                static_cast<unsigned>(battle->GetEnemies().size()),
                static_cast<unsigned>(battle->GetProjectiles().size()));
            list.Text(line, 510, 53, 0.85f, kGreen);
        } else {
            list.Text("BASE HP 10", 570, 22, 0.95f, kGreen);
        }
    }
    if (leftArm || rightArm) {
        list.Rectangle(414, 558, 452, 89, kPanel);
        if (leftArm) {
            list.Text("L ARM", 440, 570, 1.0f, kGreen);
            list.Text(ArmStatus(player.GetLeftArm().GetState()),
                440, 597, 0.9f, kGreen);
            Bar(list, 440, 625, 135,
                player.GetLeftArm().GetChargeRatio(), kGreen);
        }
        if (rightArm) {
            list.Text("R ARM", 780, 570, 1.0f, kGreen);
            list.Text(ArmStatus(player.GetRightArm().GetState()),
                780, 597, 0.9f, kGreen);
            Bar(list, 700, 625, 135,
                player.GetRightArm().GetChargeRatio(), kGreen);
        }
        list.Rectangle(631, 356, 18, 2, kGreen);
        list.Rectangle(639, 348, 2, 18, kGreen);
    }
    if (dodge) {
        std::snprintf(line, sizeof(line), "DODGE %s",
            player.GetPlayer().GetDodgeCooldownRemaining() <= 0.0f ? "READY" : "COOLING");
        list.Text(line, 600, 660, 0.9f, kGreen);
    }
    if (battle) {
        if (battle->GetPhase() == BattlePhase::Ready) {
            list.Rectangle(400, 270, 480, 120, kPanel);
            list.Text("READY - PRESS ENTER / START", 458, 310, 1.1f, kGreen);
        } else if (battle->GetPhase() == BattlePhase::Clear) {
            list.Rectangle(400, 270, 480, 120, kPanel);
            list.Text("AREA CLEAR", 565, 310, 1.3f, kGreen);
        } else if (battle->GetPhase() == BattlePhase::GameOver) {
            list.ImageFrame("Badend.png", 0, 0, 1280, 720, 7, 0);
            list.Image("retry.png", 0, 0, 1280, 720);
            list.Text("ENTER / START: RETRY", 524, 665, 0.9f, kRed);
        }
    }
}
}

void BuildFullGameUi(const FlowScreenState& flow, const TutorialSession* tutorial,
    const CombatSession* battle, const FirstPersonGame& player,
    double elapsedSeconds, float cockpitStartupRemaining, UiDrawList& list) {
    list.Clear();
    if (flow.phase == FlowPhase::Title && flow.presentation != FlowPresentation::Story) {
        list.Image("title.png", 0, 0, 1280, 720);
        if (static_cast<int>(elapsedSeconds) % 2 == 0)
            list.Image("startbar.png", 245, 520, 790, 170);
    } else if (flow.presentation == FlowPresentation::Story ||
        flow.presentation == FlowPresentation::EndingHold) {
        list.Rectangle(0, 0, 1280, 720, {0, 0, 0, 1});
        if (flow.page) {
            const float offset = flow.phase == FlowPhase::Prologue ? 200.0f : 0.0f;
            list.Image(flow.page->image, (1280.0f - flow.page->width) * 0.5f + offset,
                flow.pageY, flow.page->width, flow.page->height, {}, true);
            if (flow.page->mask) list.Rectangle(0, 500, 1280, 220, {0, 0, 0, 1});
        }
        if (flow.showPrompt) list.Text("SPACE / ENTER: NEXT     K / START: SKIP",
            480, 670, 0.85f);
    } else if (flow.phase == FlowPhase::Tutorial || flow.phase == FlowPhase::Day1 ||
        flow.phase == FlowPhase::Day2 || flow.phase == FlowPhase::Boss) {
        DrawHud(list, player, battle, tutorial, cockpitStartupRemaining);
        if (flow.phase == FlowPhase::Tutorial && tutorial) {
            list.Rectangle(348, 100, 585, 132, kPanel);
            char line[96]{};
            std::snprintf(line, sizeof(line), "PILOT OPERATION VERIFICATION %02d / 06",
                tutorial->GetStepNumber());
            list.Text(line, 360, 112, 0.95f, kGreen);
            list.Text(tutorial->GetTitle(), 360, 140, 1.2f);
            list.Text(tutorial->GetInstruction(), 360, 175, 0.85f);
            if (tutorial->IsComplete()) list.Text("ENTER / START: CONTINUE", 995, 687, 0.8f);
        }
        list.Text("F4: SKIP PHASE", 1040, 18, 0.85f);
    }
    if (flow.presentation == FlowPresentation::Closing ||
        flow.presentation == FlowPresentation::Opening) {
        list.Rectangle(0, 0, 1280, 720, {0, 0, 0, flow.curtainAmount});
    }
    if (cockpitStartupRemaining > 0.0f &&
        flow.presentation == FlowPresentation::Normal) {
        const float progress = 1.0f - cockpitStartupRemaining / 4.8f;
        list.Rectangle(0, 0, 1280, 720, {0.01f, 0.025f, 0.04f,
            0.94f * (1.0f - progress)});
        if (cockpitStartupRemaining > 1.0f) {
            list.Rectangle(300, 240, 680, 150, kPanel);
            list.Text("SYSTEM STARTUP", 505, 268, 1.4f, kGreen);
            list.Text("COCKPIT LINK / ACTUATORS / TARGET SYSTEM", 356, 310, 0.9f);
            Bar(list, 356, 345, 566, progress, kGreen);
        }
    }
}
