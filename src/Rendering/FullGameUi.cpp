#include "Rendering/FullGameUi.h"

#include "Game/FirstPersonGame.h"
#include "Gameplay/Combat/CombatSession.h"
#include "Gameplay/Tutorial/TutorialSession.h"
#include "Gameplay/Cockpit/CockpitPresentation.h"
#include "Rendering/UiRenderer.h"
#include "Collision/Collision.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {
constexpr UiColor kPanel{0.025f, 0.045f, 0.08f, 0.72f};
constexpr UiColor kGreen{0.35f, 1.0f, 0.75f, 1.0f};
constexpr UiColor kRed{1.0f, 0.35f, 0.3f, 1.0f};

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
    const PunchArm* arms[]{&player.GetLeftArm(), &player.GetRightArm()};
    for (int side = 0; side < 2; ++side) {
        const RocketPreviewTrajectory trajectory = arms[side]->GetRocketPreviewTrajectory(
            player.GetPlayer().GetPosition(), player.GetPlayer().GetBodyYaw());
        if (trajectory.flightDistance <= 0.0f) continue;
        const Vector3 end{
            trajectory.start.x + trajectory.direction.x * trajectory.flightDistance,
            trajectory.start.y + trajectory.direction.y * trajectory.flightDistance,
            trajectory.start.z + trajectory.direction.z * trajectory.flightDistance};
        struct LockCandidate {
            const FirstPersonTarget* target;
            float time;
        };
        std::vector<LockCandidate> candidates;
        for (const FirstPersonTarget& target : player.GetTargets()) {
            if (target.hitPoints <= 0) continue;
            const Collision::SweepHit hit = Collision::SweepSphereAgainstSphere(
                trajectory.start, end, FirstPersonTuning::kFistRadius,
                {target.position, target.radius});
            if (hit.didHit) candidates.push_back({&target, hit.time});
        }
        std::sort(candidates.begin(), candidates.end(),
            [](const LockCandidate& a, const LockCandidate& b) {
                return a.time < b.time;
            });
        const UiColor tint = side == 0 ? UiColor{0.38f, 1.0f, 0.86f, 0.9f} :
            UiColor{0.55f, 0.85f, 1.0f, 0.9f};
        for (std::size_t index = 0; index < candidates.size(); ++index) {
            const FirstPersonTarget& target = *candidates[index].target;
            const float targetHeight = target.id.starts_with("target_battle_boss") ||
                target.id.starts_with("target_battle_enemy") ? 1.4f : 0.0f;
            const Vector3 relative{target.position.x - eye.x,
                target.position.y + targetHeight - eye.y, target.position.z - eye.z};
            const float depth = relative.x * forward.x + relative.y * forward.y +
                relative.z * forward.z;
            if (depth <= 0.1f) continue;
            const float x = 640.0f + focalLength *
                (relative.x * right.x + relative.z * right.z) / depth;
            const float y = 360.0f - focalLength *
                (relative.x * up.x + relative.y * up.y + relative.z * up.z) / depth;
            if (x < 32.0f || x > 1248.0f || y < 40.0f || y > 680.0f) continue;
            list.Rectangle(x - 32, y - 32, 64, 3, tint);
            list.Rectangle(x - 32, y + 29, 64, 3, tint);
            list.Rectangle(x - 32, y - 32, 3, 64, tint);
            list.Rectangle(x + 29, y - 32, 3, 64, tint);
            char label[24]{};
            std::snprintf(label, sizeof(label), "%c LOCK %u",
                side == 0 ? 'L' : 'R', static_cast<unsigned>(index + 1));
            list.Text(label, x - 30, y - 57, 0.8f, tint);
        }
    }
}
void DrawHud(UiDrawList& list, const FirstPersonGame& player,
    const CombatSession* battle, const TutorialSession* tutorial,
    const CockpitPresentation& cockpit) {
    bool base = true;
    bool armor = !tutorial || tutorial->IsCockpitConnected();
    bool shields = !tutorial || tutorial->GetStep() >= TutorialStep::Defense;
    bool leftArm = !tutorial || tutorial->GetStep() >= TutorialStep::Punch;
    bool rightArm = leftArm;
    bool dodge = !tutorial || tutorial->GetStep() >= TutorialStep::Dodge;
    bool targets = !tutorial || tutorial->GetStep() >= TutorialStep::RocketPunch;
    if (cockpit.IsStartupActive()) {
        const CockpitHudVisibility visible = cockpit.GetHudVisibility();
        base = visible.base;
        armor = visible.armor;
        shields = visible.shield;
        leftArm = visible.leftArm;
        rightArm = visible.rightArm;
        dodge = visible.dodge;
        targets = visible.targets;
    }
    char line[128]{};
    if (targets) TargetFrames(list, player);
    if (armor || shields) list.Rectangle(31.0f,
        shields ? 579.0f : 620.0f, 326.0f,
        shields ? 116.0f : 73.0f, kPanel);
    if (armor) {
        const auto& vitality = player.GetVitality();
        const float ratio = static_cast<float>(vitality.GetHitPoints()) /
            std::max(1, vitality.GetMaximumHitPoints());
        std::snprintf(line, sizeof(line), "%03d",
            static_cast<int>(std::round(ratio * 100.0f)));
        list.Text("ARMOR", 44, 628, 1.0f);
        list.Text(line, 122, 628, 1.0f);
        Segments(list, 164, 635, 10,
            static_cast<int>(std::round(ratio * 10.0f)), 13,
            {0.78f, 0.93f, 1.0f, 0.95f});
    }
    if (shields) {
        list.Text("SHIELD", 44, 590, 1.0f, kGreen);
        Segments(list, 132, 595, 3,
            player.GetVitality().GetShieldLayers(), 34,
            {0.36f, 0.88f, 1.0f, 0.95f});
        const bool full = player.GetVitality().GetShieldLayers() >=
            player.GetVitality().GetMaximumShieldLayers();
        if (full) list.Text("FULL", 280, 590, 1.0f);
        else {
            std::snprintf(line, sizeof(line), "%d%%", static_cast<int>(
                std::round(player.GetVitality().GetShieldRechargeRatio() * 100.0f)));
            list.Text(line, 280, 590, 1.0f);
        }
        list.Rectangle(132, 613, 189, 6, {0.20f, 0.38f, 0.48f, 0.8f});
        list.Rectangle(132, 613, 189 * (full ? 1.0f :
            player.GetVitality().GetShieldRechargeRatio()), 6,
            {0.36f, 0.88f, 1.0f, 0.95f});
    }
    if (base && (battle || tutorial)) {
        list.Rectangle(486, 8, 320, 92, kPanel);
        const int hp = battle ? battle->GetBaseHitPoints() : 10;
        const int maximum = battle ? battle->GetBaseMaximumHitPoints() : 10;
        const float ratio = static_cast<float>(hp) / std::max(1, maximum);
        std::snprintf(line, sizeof(line), "%03d / 1000",
            static_cast<int>(std::round(ratio * 1000.0f)));
        list.Text("BASE HP", 600, 12, 1.2f);
        list.Text(line, 570, 38, 1.2f);
        Segments(list, 500, 68, 15,
            static_cast<int>(std::round(ratio * 15.0f)), 16,
            {0.42f, 1.0f, 0.62f, 0.95f});
    }
    if (leftArm || rightArm) {
        if (leftArm) {
            list.Text("L ARM", 440, 570, 1.3f);
            list.Text(ArmStatus(player.GetLeftArm().GetState()),
                440, 598, 1.3f);
            list.RotatedRectangle(420, 608, 14, 14, 0.7853982f,
                {0.02f, 0.05f, 0.08f, 0.92f});
            list.RotatedRectangle(420, 608, 8, 8, 0.7853982f,
                player.GetLeftArm().GetState() == PunchArmState::Ready
                ? kGreen : UiColor{1.0f, 0.82f, 0.20f, 0.98f});
        }
        if (rightArm) {
            list.Text("R ARM", 780, 570, 1.3f);
            list.Text(ArmStatus(player.GetRightArm().GetState()),
                780, 598, 1.3f);
            list.RotatedRectangle(760, 608, 14, 14, 0.7853982f,
                {0.02f, 0.05f, 0.08f, 0.92f});
            list.RotatedRectangle(760, 608, 8, 8, 0.7853982f,
                player.GetRightArm().GetState() == PunchArmState::Ready
                ? kGreen : UiColor{1.0f, 0.82f, 0.20f, 0.98f});
        }
    }
    if (dodge) {
        const bool ready = player.GetPlayer().GetDodgeCooldownRemaining() <= 0.0f;
        list.Text("DODGE", 600, 610, 1.3f);
        list.Text(ready ? "READY" : "RECHARGING", 600, 638, 1.3f);
        list.RotatedRectangle(580, 648, 14, 14, 0.7853982f,
            {0.02f, 0.05f, 0.08f, 0.92f});
        list.RotatedRectangle(580, 648, 8, 8, 0.7853982f,
            ready ? kGreen : kRed);
    }
    if (battle && !battle->IsPractice()) {
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
    double elapsedSeconds, const CockpitPresentation& cockpit,
    bool practiceDataVisible, UiDrawList& list) {
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
        DrawHud(list, player, battle, tutorial, cockpit);
        if (flow.phase == FlowPhase::Tutorial && tutorial &&
            !cockpit.IsActive() &&
            (!tutorial->IsComplete() || tutorial->ShowFreePracticeCaption())) {
            list.Rectangle(348, 100, 585, 134, kPanel);
            char line[96]{};
            std::snprintf(line, sizeof(line), "PILOT OPERATION VERIFICATION  %02d / 06",
                tutorial->GetStepNumber());
            list.Text(line, 360, 112, 1.0f);
            list.Text(tutorial->GetTitle(), 360, 140, 1.25f);
            if (tutorial->IsConfirming()) {
                list.Text("RESPONSE CONFIRMED  /  CHECK PASSED", 360, 168);
            } else if (tutorial->GetStep() == TutorialStep::VisualLink &&
                tutorial->GetLookAngle() < 1.5707963f) {
                std::snprintf(line, sizeof(line),
                    "RIGHT STICK / MOUSE: VISUAL SWEEP  %03d%%",
                    static_cast<int>(std::round(std::clamp(
                        tutorial->GetLookAngle() / 1.5707963f, 0.0f, 1.0f) * 100.0f)));
                list.Text(line, 360, 168);
            } else if (tutorial->GetStep() == TutorialStep::Mobility) {
                std::snprintf(line, sizeof(line), "MOVE %.1f / 5.0   VIEW %03d%%",
                    tutorial->GetMoveDistance(), static_cast<int>(std::round(
                        std::clamp(tutorial->GetLookAngle() / 0.5235988f,
                            0.0f, 1.0f) * 100.0f)));
                list.Text(line, 360, 168);
                list.Text("LEFT STICK / WASD: MOVE   RIGHT STICK / MOUSE: LOOK",
                    360, 190);
            } else if (tutorial->GetStep() == TutorialStep::Punch ||
                tutorial->GetStep() == TutorialStep::RocketPunch) {
                std::snprintf(line, sizeof(line), "L %s   R %s",
                    tutorial->HasLeftHit() ? "PASS" : "WAIT",
                    tutorial->HasRightHit() ? "PASS" : "WAIT");
                list.Text(tutorial->GetInstruction(), 360, 168);
                list.Text(line, 360, 190);
            } else {
                list.Text(tutorial->GetInstruction(), 360, 168);
                if (tutorial->IsComplete())
                    list.Text("FREE PRACTICE - ALL FUNCTIONS UNLOCKED", 360, 190);
            }
            if (tutorial->IsComplete())
                list.Text("ENTER / START: CONTINUE", 995, 687, 0.8f);
        }
        if (battle && battle->IsPractice() && practiceDataVisible) {
            char line[160]{};
            list.Rectangle(6, 7, 468, 128, kPanel);
            list.Text("F1 CAMERA  F2 MODEL  F3 DATA", 16, 16, 0.8f);
            list.Text("F5 FULL  F6 QUICK  F7 BOSS  F8 REBOOT", 16, 37, 0.8f);
            list.Text("1-7 ENEMY  SHIFT+1-4 PROJECTILES", 16, 58, 0.8f);
            list.Text("0/8/9/B/C/N/M BOSS  R RESET", 16, 79, 0.8f);
            std::snprintf(line, sizeof(line), "Enemy %u | Projectile %u | Pattern %s | HP %d | Shield %d",
                static_cast<unsigned>(battle->GetEnemies().size()),
                static_cast<unsigned>(battle->GetProjectiles().size()),
                battle->GetPracticePatternName(),
                player.GetVitality().GetHitPoints(),
                player.GetVitality().GetShieldLayers());
            list.Text(line, 16, 105, 0.7f);
        }
        list.Text("F4: SKIP PHASE", 1040, 18, 0.85f);
    }
    if (flow.presentation == FlowPresentation::Closing ||
        flow.presentation == FlowPresentation::Opening) {
        list.Rectangle(0, 0, 1280, 720, {0, 0, 0, flow.curtainAmount});
    }
    if (cockpit.IsActive() && flow.presentation == FlowPresentation::Normal) {
        list.Rectangle(0, 0, 1280, 720,
            {0.0f, 0.0f, 0.0f, cockpit.GetOverlayAlpha()});
        if (cockpit.IsStartupActive()) {
            list.Text(cockpit.GetTitle(), 460, 152, 1.2f);
            for (int index = 0; index < cockpit.GetVisibleLineCount(); ++index)
                list.Text(cockpit.GetLine(index), 430, 220 + index * 32.0f);
            if (cockpit.IsAllGreenVisible()) {
                list.Text("SYSTEM ALL GREEN", 470, 465, 1.8f);
                if (cockpit.GetMode() == CockpitMode::FullStartup) {
                    list.Text("SUPER ROBOT BEAST G", 450, 530, 1.4f);
                    list.Text("START UP", 550, 570, 1.5f);
                }
            }
        } else if (cockpit.GetMode() == CockpitMode::EmergencyReboot) {
            if (cockpit.GetElapsed() < 0.5f) {
                list.Text("WARNING", 560, 260, 1.8f);
                list.Text("SYSTEM OVERLOAD", 490, 315, 2.0f);
            } else if (cockpit.IsAllGreenVisible()) {
                list.Text("REBOOTING...", 525, 230, 1.6f);
                list.Text("MAIN POWER      OK", 470, 310, 1.1f);
                list.Text("CONTROL         OK", 470, 345, 1.1f);
                list.Text("ARM SYSTEM      OK", 470, 380, 1.1f);
                list.Text("SYSTEM ALL GREEN", 470, 465, 1.8f);
            } else {
                list.Text("SYSTEM OFFLINE", 500, 280, 2.0f);
                list.Text("MAIN POWER      --", 470, 350, 1.1f);
                list.Text("CONTROL         LOST", 470, 385, 1.1f);
                list.Text("ARM SYSTEM      OFFLINE", 470, 420, 1.1f);
            }
        }
    }
}
