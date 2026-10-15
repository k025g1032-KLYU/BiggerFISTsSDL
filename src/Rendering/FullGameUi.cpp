#include "Rendering/FullGameUi.h"

#include "Game/FirstPersonGame.h"
#include "Gameplay/Combat/CombatSession.h"
#include "Gameplay/Tutorial/TutorialSession.h"
#include "Gameplay/Cockpit/CockpitPresentation.h"
#include "Rendering/UiRenderer.h"
#include "Rendering/UiLayout.h"
#include "Collision/Collision.h"
#include "Gameplay/Player/FirstPersonTuning.h"
#include "World/ModelScene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

namespace {
constexpr UiColor kPanel{0.025f, 0.045f, 0.08f, 0.72f};
constexpr UiColor kGreen{0.26f, 1.0f, 0.50f, 0.98f};
constexpr UiColor kRed{1.0f, 0.20f, 0.16f, 0.98f};
constexpr UiColor kArmIdle{0.42f, 0.52f, 0.60f, 0.95f};
constexpr UiColor kArmCharging{1.0f, 0.82f, 0.20f, 0.98f};
constexpr UiColor kArmReady{0.26f, 1.0f, 0.50f, 0.98f};
constexpr UiColor kArmDeployed{1.0f, 0.34f, 0.12f, 0.98f};

void Segments(UiDrawList& list, float x, float y, int count, int filled,
    float width, UiColor color, UiColor emptyColor, float gap = 3.0f) {
    for (int index = 0; index < count; ++index) {
        list.Rectangle(x + index * (width + gap), y, width, 10.0f,
            index < filled ? color : emptyColor);
    }
}
const char* ArmStatus(PunchArmState state) {
    switch (state) {
    case PunchArmState::Ready: return "IDLE";
    case PunchArmState::Charging: return "CHARGING";
    case PunchArmState::Charged: return "READY";
    default: return "DEPLOYED";
    }
}
UiColor ArmColor(PunchArmState state) {
    switch (state) {
    case PunchArmState::Ready: return kArmIdle;
    case PunchArmState::Charging: return kArmCharging;
    case PunchArmState::Charged: return kArmReady;
    default: return kArmDeployed;
    }
}
void TargetFrames(UiDrawList& list, const FirstPersonGame& player,
    const ModelScene& scene, const TutorialSession* tutorial,
    int outputWidth, int outputHeight) {
    if (outputWidth <= 0 || outputHeight <= 0) return;
    const UiLayout layout(static_cast<float>(outputWidth),
        static_cast<float>(outputHeight));
    Matrix4x4 view{}, projection{};
    if (!TryMakeLookAtLHMatrix(scene.cameraPosition, scene.cameraTarget,
            scene.cameraUp, view) ||
        !TryMakePerspectiveLHMatrix(scene.verticalFovDegrees *
            std::numbers::pi_v<float> / 180.0f,
            static_cast<float>(outputWidth) / outputHeight,
            scene.nearPlane, scene.farPlane, projection)) return;
    const Matrix4x4 viewProjection = MultiplyMatrices(view, projection);
    const PunchArm* arms[]{&player.GetLeftArm(), &player.GetRightArm()};
    for (int side = 0; side < 2; ++side) {
        const PunchArmState state = arms[side]->GetState();
        if (state != PunchArmState::Ready &&
            !((state == PunchArmState::Charging || state == PunchArmState::Charged) &&
                arms[side]->GetAttackType() == PunchArmAttackType::RocketPunch))
            continue;
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
            if (tutorial && !tutorial->ShouldShowTargets() &&
                target.displayKind == TargetDisplayKind::Static) continue;
            const Collision::SweepHit hit = Collision::SweepSphereAgainstSphere(
                trajectory.start, end, FirstPersonTuning::kFistRadius,
                {target.position, target.radius});
            if (hit.didHit) candidates.push_back({&target, hit.time});
        }
        std::sort(candidates.begin(), candidates.end(),
            [](const LockCandidate& a, const LockCandidate& b) {
                return a.time < b.time;
            });
        const UiColor tint = side == 0 ? UiColor{0.20f, 0.85f, 1.0f, 0.95f} :
            UiColor{1.0f, 0.55f, 0.12f, 0.95f};
        for (std::size_t index = 0; index < candidates.size(); ++index) {
            const FirstPersonTarget& target = *candidates[index].target;
            const float targetHeight = target.displayKind == TargetDisplayKind::Enemy ||
                target.displayKind == TargetDisplayKind::Boss ? 1.4f : 0.0f;
            const float alpha = scene.interpolationAlpha;
            const float point[4]{
                target.previousPosition.x + (target.position.x - target.previousPosition.x) * alpha,
                target.previousPosition.y + (target.position.y - target.previousPosition.y) * alpha + targetHeight,
                target.previousPosition.z + (target.position.z - target.previousPosition.z) * alpha, 1.0f};
            float clip[4]{};
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    clip[column] += point[row] * viewProjection.elements[row][column];
            if (clip[3] <= 0.0f) continue;
            const float ndcX = clip[0] / clip[3];
            const float ndcY = clip[1] / clip[3];
            const float ndcZ = clip[2] / clip[3];
            if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f ||
                ndcY > 1.0f || ndcZ < 0.0f || ndcZ > 1.0f) continue;
            const float overlapOffset = side == 0 ? -1.5f : 1.5f;
            const float screenX = (ndcX + 1.0f) * layout.width * 0.5f +
                overlapOffset * layout.safeScale;
            const float screenY = (1.0f - ndcY) * layout.height * 0.5f +
                overlapOffset * layout.safeScale;
            if (screenX < 32.0f * layout.safeScale ||
                screenX > layout.width - 32.0f * layout.safeScale ||
                screenY < 40.0f * layout.safeScale ||
                screenY > layout.height - 40.0f * layout.safeScale) continue;
            const float x = (screenX - layout.safeOffsetX) /
                layout.safeScale;
            const float y = (screenY - layout.safeOffsetY) /
                layout.safeScale;
            list.Rectangle(x - 32, y - 32, 64, 3, tint);
            list.Rectangle(x - 32, y + 29, 64, 3, tint);
            list.Rectangle(x - 32, y - 32, 3, 64, tint);
            list.Rectangle(x + 29, y - 32, 3, 64, tint);
            char label[24]{};
            std::snprintf(label, sizeof(label), "%c LOCK %u",
                side == 0 ? 'L' : 'R', static_cast<unsigned>(index + 1));
            list.Text(label, x - 30, y - 56, 1.0f, tint);
        }
    }
}
void DrawHud(UiDrawList& list, const FirstPersonGame& player,
    const CombatSession* battle, const TutorialSession* tutorial,
    const CockpitPresentation& cockpit, const ModelScene& scene,
    bool showBattleStatus, bool combatDataVisible,
    int outputWidth, int outputHeight) {
    bool base = true;
    bool armor = !tutorial || tutorial->IsCockpitConnected();
    bool shields = !tutorial || tutorial->GetStep() >= TutorialStep::Defense;
    bool leftArm = !tutorial || tutorial->GetStep() >= TutorialStep::Punch;
    bool rightArm = leftArm;
    bool dodge = !tutorial || tutorial->GetStep() >= TutorialStep::Dodge;
    bool targets = !tutorial || tutorial->GetStep() >= TutorialStep::RocketPunch;
    if (!tutorial && cockpit.IsStartupActive()) {
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
    if (showBattleStatus && player.IsSystemOffline()) {
        std::snprintf(line, sizeof(line), "SYSTEM OFFLINE %.1f",
            static_cast<double>(player.GetOfflineRemaining()));
        list.Text(line, 470, 42, 2.0f);
    }
    if (targets) TargetFrames(list, player, scene, tutorial,
        outputWidth, outputHeight);
    if (base && (battle || tutorial)) {
        const int hp = battle ? battle->GetBaseHitPoints() : 10;
        const int maximum = battle ? battle->GetBaseMaximumHitPoints() : 10;
        const float ratio = static_cast<float>(hp) / std::max(1, maximum);
        std::snprintf(line, sizeof(line), "%03d / 1000",
            static_cast<int>(std::round(ratio * 1000.0f)));
        list.Text("BASE HP", 600, 12, 1.2f);
        list.Text(line, 570, 38, 1.2f);
        Segments(list, 500, 68, 15,
            static_cast<int>(std::round(ratio * 15.0f)), 16,
            {0.42f, 1.0f, 0.62f, 0.95f},
            {0.12f, 0.32f, 0.19f, 0.75f});
    }
    list.SetPlacement(UiPlacement::LeftEdge);
    if (armor) {
        const auto& vitality = player.GetVitality();
        const float ratio = static_cast<float>(vitality.GetHitPoints()) /
            std::max(1, vitality.GetMaximumHitPoints());
        std::snprintf(line, sizeof(line), "%03d",
            static_cast<int>(std::round(ratio * 100.0f)));
        list.Text("ARMOR", 44, 628, 1.2f);
        list.Text(line, 122, 628, 1.2f);
        Segments(list, 164, 634, 10,
            static_cast<int>(std::round(ratio * 10.0f)), 13,
            {0.78f, 0.93f, 1.0f, 0.95f},
            {0.18f, 0.28f, 0.36f, 0.75f});
    }
    if (shields) {
        const int layers = player.GetVitality().GetShieldLayers();
        const UiColor shieldColor = layers >= player.GetVitality().GetMaximumShieldLayers()
            ? UiColor{0.36f, 0.88f, 1.0f, 0.95f}
            : layers >= 2 ? UiColor{1.0f, 0.82f, 0.25f, 0.95f}
            : UiColor{1.0f, 0.26f, 0.20f, 0.95f};
        list.Text("SHIELD", 44, 588, 1.2f);
        Segments(list, 132, 593, 3,
            layers, 34, shieldColor,
            {0.20f, 0.38f, 0.48f, 0.80f}, 5.0f);
        const bool full = player.GetVitality().GetShieldLayers() >=
            player.GetVitality().GetMaximumShieldLayers();
        if (full) list.Text("FULL", 280, 588, 1.2f);
        else {
            std::snprintf(line, sizeof(line), "%d%%", static_cast<int>(
                std::round(player.GetVitality().GetShieldRechargeRatio() * 100.0f)));
            list.Text(line, 280, 588, 1.2f);
        }
        list.Rectangle(132, 613, 189, 6, {0.20f, 0.38f, 0.48f, 0.8f});
        list.Rectangle(132, 613, 189 * (full ? 1.0f :
            player.GetVitality().GetShieldRechargeRatio()), 6,
            shieldColor);
    }
    list.SetPlacement(UiPlacement::Safe);
    if (leftArm || rightArm) {
        if (leftArm) {
            list.Text("L ARM", 440, 570, 1.3f);
            list.Text(ArmStatus(player.GetLeftArm().GetState()),
                440, 598, 1.3f);
            list.RotatedRectangle(420, 608, 14, 14, 0.7853982f,
                {0.02f, 0.05f, 0.08f, 0.92f});
            list.RotatedRectangle(420, 608, 8, 8, 0.7853982f,
                ArmColor(player.GetLeftArm().GetState()));
        }
        if (rightArm) {
            list.Text("R ARM", 780, 570, 1.3f);
            list.Text(ArmStatus(player.GetRightArm().GetState()),
                780, 598, 1.3f);
            list.RotatedRectangle(760, 608, 14, 14, 0.7853982f,
                {0.02f, 0.05f, 0.08f, 0.92f});
            list.RotatedRectangle(760, 608, 8, 8, 0.7853982f,
                ArmColor(player.GetRightArm().GetState()));
        }
    }
    if (dodge) {
        const bool ready = player.GetPlayer().IsDodgeReady();
        list.Text("DODGE", 600, 610, 1.3f);
        list.Text(ready ? "READY" : "RECHARGING", 600, 638, 1.3f);
        list.RotatedRectangle(580, 648, 14, 14, 0.7853982f,
            {0.02f, 0.05f, 0.08f, 0.92f});
        list.RotatedRectangle(580, 648, 8, 8, 0.7853982f,
            ready ? kGreen : kRed);
    }
    if (showBattleStatus && battle) {
        if (const BattleBoss* boss = battle->GetBoss()) {
            if (!combatDataVisible) {
                std::snprintf(line, sizeof(line), "BOSS CORE %02d / %02d",
                    boss->GetHitPoints(), boss->GetMaximumHitPoints());
                list.Text(line, 520, 96, 1.1f);
            }
        }
    }
    if (showBattleStatus && battle && !battle->IsPractice()) {
        if (battle->GetPhase() == BattlePhase::Ready) {
            list.Text("READY - PRESS ENTER / GAMEPAD START TO START",
                405, 300, 1.5f);
        } else if (battle->GetPhase() == BattlePhase::Countdown) {
            std::snprintf(line, sizeof(line), "%d", std::clamp(
                static_cast<int>(std::ceil(battle->GetCountdownRemaining())), 1, 3));
            list.Text(line, 620, 280, 3.0f);
        } else if (battle->GetPhase() == BattlePhase::Clear) {
            list.Text("MISSION CLEAR", 500, 270, 2.5f);
        } else if (battle->GetPhase() == BattlePhase::GameOver) {
            list.Text("MISSION FAILED", 485, 270, 2.5f);
            list.Text("ENTER / START TO RETRY", 475, 326, 1.25f);
        }
    }
}

void DrawCockpitOverlay(UiDrawList& list,
    const CockpitPresentation& cockpit) {
    list.SetPlacement(UiPlacement::FullViewport);
    list.Rectangle(0, 0, 1280, 720,
        {0.0f, 0.0f, 0.0f, cockpit.GetOverlayAlpha()});
    list.SetPlacement(UiPlacement::Safe);
}

void DrawCockpitPresentation(UiDrawList& list,
    const CockpitPresentation& cockpit) {
    DrawCockpitOverlay(list, cockpit);
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

const char* DodgeStateName(PlayerDodgeState state) {
    switch (state) {
    case PlayerDodgeState::Ready: return "Ready";
    case PlayerDodgeState::Boosting: return "Boosting";
    case PlayerDodgeState::Sliding: return "Sliding";
    case PlayerDodgeState::Recovery: return "Recovery";
    }
    return "Ready";
}

const char* DebugArmStateName(PunchArmState state) {
    switch (state) {
    case PunchArmState::Ready: return "Ready";
    case PunchArmState::Charging:
    case PunchArmState::Charged: return "Charging";
    case PunchArmState::Punching: return "Punching";
    case PunchArmState::RocketFlying:
    case PunchArmState::RocketEvacuating: return "Away";
    case PunchArmState::Returning: return "Returning";
    case PunchArmState::Recovery: return "Recovery";
    }
    return "Unknown";
}

const char* ImpactStateName(PlayerImpactState state) {
    switch (state) {
    case PlayerImpactState::None: return "None";
    case PlayerImpactState::Holding: return "Holding";
    case PlayerImpactState::Knockback: return "Knockback";
    case PlayerImpactState::NeedleStun: return "NeedleStun";
    }
    return "None";
}

const char* UltimateStateName(UltimateState state) {
    switch (state) {
    case UltimateState::Idle: return "Idle";
    case UltimateState::Forming: return "Forming";
    case UltimateState::Ready: return "Ready";
    case UltimateState::Flying: return "Flying";
    case UltimateState::Returning: return "Returning";
    case UltimateState::Offline: return "Offline";
    }
    return "Idle";
}

const char* ChargeStateName(BattleEnemy::ChargeState state) {
    switch (state) {
    case BattleEnemy::ChargeState::Approaching: return "Approaching";
    case BattleEnemy::ChargeState::Repositioning: return "Repositioning";
    case BattleEnemy::ChargeState::ReacquiringTarget: return "ReacquiringTarget";
    case BattleEnemy::ChargeState::Preparing: return "Preparing";
    case BattleEnemy::ChargeState::Charging: return "Charging";
    case BattleEnemy::ChargeState::Impact: return "Impact";
    case BattleEnemy::ChargeState::HitBraking: return "HitBraking";
    case BattleEnemy::ChargeState::MissBraking: return "MissBraking";
    }
    return "None";
}

void DrawCombatData(UiDrawList& list, const CombatSession* battle,
    FlowPhase phase, const FirstPersonGame& player, const ModelScene& scene) {
    char line[256]{};
    list.Text("Combat Prototype v0.4", 16, 16);
    list.Text("Enter / Gamepad START Start-Retry | WASD Move | Space Dodge | Mouse Look | V Observe | F1 Third Person | F3/View Data | R Reset", 16, 38);
    list.Text("Mouse: L/R Rocket | Shift+L/R Punch | Q/E Backup Punch | Gamepad: LS Move | A Dodge | RS Look | R3 Observe | LT/RT Punch | LB/RB Rocket", 16, 60);
    if (phase == FlowPhase::Tutorial && battle && battle->IsPractice()) {
        list.Text("Ready Debug: 0 Boss | 8 Cycle Needles | 9 Cone Charge | B Boxer | C Needle Combo | N Full Barrage | M Spread Burst | 1 Missile | 2 Bomber | 3 Charge | 4/5/6 Tests | 7 Mixed", 16, 82);
        list.Text("Ready Debug: Shift+1 Single | Shift+2 Line | Shift+3 Bombs | Shift+4 Mixed", 16, 104);
        list.Text("Presentation Debug: F5 Full | F6 Quick | F7 Boss | F8 Shutdown-Reboot", 760, 148);
    }
    list.Text(phase == FlowPhase::Day1 ? "SESSION: DAY1" :
        phase == FlowPhase::Day2 ? "SESSION: DAY2" :
        phase == FlowPhase::Boss ? "SESSION: BOSS" : "SESSION: TUTORIAL",
        760, 16);

    const Vector3& camera = player.GetCamera().GetPosition();
    const bool thirdPerson = std::hypot(scene.cameraPosition.x - camera.x,
        scene.cameraPosition.z - camera.z) > 3.0f;
    const char* view = thirdPerson ? "Third Person" : "First Person";
    const char* control = player.GetCamera().GetControlMode() ==
        CameraControlMode::Observation ? "Observation" : "Driving";
    std::snprintf(line, sizeof(line), "VIEW: %s | OBSERVATION: %s",
        view, player.GetCamera().GetControlMode() ==
            CameraControlMode::Observation ? "ON" : "OFF");
    list.Text(line, 760, 126);
    if (player.GetCamera().GetControlMode() == CameraControlMode::Observation) {
        const float yaw = player.GetPlayer().GetBodyYaw();
        std::snprintf(line, sizeof(line),
            "OBSERVATION | BODY FRONT / ATTACK: X %+.2f Z %+.2f",
            static_cast<double>(std::sin(yaw)),
            static_cast<double>(std::cos(yaw)));
        list.Text(line, 700, 170);
    }

    const PunchArm* arms[]{&player.GetLeftArm(), &player.GetRightArm()};
    for (int side = 0; side < 2; ++side) {
        std::snprintf(line, sizeof(line),
            "%s Arm: %-12s Charge: %3.0f%% Rocket: %s",
            side == 0 ? "Left" : "Right",
            DebugArmStateName(arms[side]->GetState()),
            static_cast<double>(arms[side]->GetChargeRatio() * 100.0f),
            arms[side]->IsFistAway() ? "Away" : "Connected");
        list.Text(line, 16, 132 + side * 22.0f);
    }
    const auto dummy = std::count_if(player.GetTargets().begin(),
        player.GetTargets().end(), [](const FirstPersonTarget& target) {
            return target.displayKind == TargetDisplayKind::Static &&
                target.hitPoints > 0;
        });
    std::snprintf(line, sizeof(line), "Dummy Count: %zu / %zu",
        static_cast<std::size_t>(dummy),
        static_cast<std::size_t>(std::count_if(player.GetTargets().begin(),
            player.GetTargets().end(), [](const FirstPersonTarget& target) {
                return target.displayKind == TargetDisplayKind::Static;
            })));
    list.Text(line, 16, 176);
    const PlayerVitality& vitality = player.GetVitality();
    std::snprintf(line, sizeof(line),
        "Player HP: %d / %d | Shield: %d / %d | Recharge: %3.0f%%",
        vitality.GetHitPoints(), vitality.GetMaximumHitPoints(),
        vitality.GetShieldLayers(), vitality.GetMaximumShieldLayers(),
        static_cast<double>(vitality.GetShieldRechargeRatio() * 100.0f));
    list.Text(line, 16, 198);
    if (battle) {
        std::snprintf(line, sizeof(line), "Research Institute HP: %d / %d",
            battle->GetBaseHitPoints(), battle->GetBaseMaximumHitPoints());
        list.Text(line, 16, 220);

    const auto countEnemy = [&](BattleEnemyType type) {
        return std::count_if(battle->GetEnemies().begin(), battle->GetEnemies().end(),
            [type](const BattleEnemy& enemy) {
                return !enemy.defeated && enemy.type == type;
            });
    };
    const auto activeEnemy = std::count_if(battle->GetEnemies().begin(),
        battle->GetEnemies().end(), [](const BattleEnemy& enemy) {
            return !enemy.defeated;
        });
    std::snprintf(line, sizeof(line),
        "Active Enemies: %zu | MissileUnit %zu | Bomber %zu | ChargeUnit %zu",
        static_cast<std::size_t>(activeEnemy),
        static_cast<std::size_t>(countEnemy(BattleEnemyType::MissileUnit)),
        static_cast<std::size_t>(countEnemy(BattleEnemyType::Bomber)),
        static_cast<std::size_t>(countEnemy(BattleEnemyType::ChargeUnit)));
    list.Text(line, 16, 242);
    const auto charge = std::find_if(battle->GetEnemies().begin(),
        battle->GetEnemies().end(), [](const BattleEnemy& enemy) {
            return !enemy.defeated && enemy.type == BattleEnemyType::ChargeUnit;
        });
    std::snprintf(line, sizeof(line), "ChargeUnit State: %s",
        charge == battle->GetEnemies().end() ? "None" :
            ChargeStateName(charge->chargeState));
    list.Text(line, 16, 264);
    const auto countProjectile = [&](BattleProjectileType type) {
        return std::count_if(battle->GetProjectiles().begin(),
            battle->GetProjectiles().end(), [type](const BattleProjectile& projectile) {
                return projectile.type == type;
            });
    };
    std::snprintf(line, sizeof(line),
        "Active Projectiles: Missile %zu | FloatingBomb %zu | Crystal Spike %zu | Crystal Cube %zu",
        static_cast<std::size_t>(countProjectile(BattleProjectileType::Missile)),
        static_cast<std::size_t>(countProjectile(BattleProjectileType::FloatingBomb)),
        static_cast<std::size_t>(countProjectile(BattleProjectileType::CrystalSpike)),
        static_cast<std::size_t>(countProjectile(BattleProjectileType::CrystalCube)));
    list.Text(line, 16, 286);
    std::snprintf(line, sizeof(line), "Enemy Pattern: %s | Projectile Pattern: %s",
        battle->GetEnemyPracticePatternName(),
        battle->GetProjectilePracticePatternName());
    if (battle->IsPractice()) list.Text(line, 16, 308);
    std::snprintf(line, sizeof(line), "Combat Height Band: %.2f - %.2f",
        static_cast<double>(battle->GetConfig().enemy.combatHeightMinimum),
        static_cast<double>(battle->GetConfig().enemy.combatHeightMaximum));
    list.Text(line, 16, 330);
    }
    std::snprintf(line, sizeof(line), "Camera: %s | Control: %s | V/R3",
        view, control);
    list.Text(line, 16, 352);
    const PlayerMovement& movement = player.GetPlayer();
    std::snprintf(line, sizeof(line), "Dodge: %-9s | %s %.2f | State Timer %.2f",
        DodgeStateName(movement.GetDodgeState()),
        movement.IsDodgeReady() ? "Ready" : "Cooldown",
        static_cast<double>(movement.GetDodgeCooldownRemaining()),
        static_cast<double>(movement.GetDodgeStateTimer()));
    list.Text(line, 16, 374);
    const Vector3& velocity = movement.GetVelocity();
    std::snprintf(line, sizeof(line),
        "Horizontal Velocity: X %.2f Z %.2f | Speed %.2f",
        static_cast<double>(velocity.x), static_cast<double>(velocity.z),
        static_cast<double>(std::hypot(velocity.x, velocity.z)));
    list.Text(line, 16, 396);
    const Vector3& direction = movement.GetDodgeDirection();
    std::snprintf(line, sizeof(line), "Dodge Direction: X %.2f Z %.2f",
        static_cast<double>(direction.x), static_cast<double>(direction.z));
    list.Text(line, 16, 418);
    std::snprintf(line, sizeof(line),
        "Player Bounds: X %.0f..%.0f | Z %.0f..%.0f | Enabled",
        static_cast<double>(FirstPersonTuning::kBattlefieldMinimumX),
        static_cast<double>(FirstPersonTuning::kBattlefieldMaximumX),
        static_cast<double>(FirstPersonTuning::kBattlefieldMinimumZ),
        static_cast<double>(FirstPersonTuning::kBattlefieldMaximumZ));
    list.Text(line, 16, 440);
    if (battle) {
        std::snprintf(line, sizeof(line),
            "Blockout: X %.0f..%.0f Z %.0f..%.0f | Deploy Z %.0f..%.0f",
            static_cast<double>(FirstPersonTuning::kBattlefieldMinimumX),
            static_cast<double>(FirstPersonTuning::kBattlefieldMaximumX),
            static_cast<double>(FirstPersonTuning::kBattlefieldMinimumZ),
            static_cast<double>(FirstPersonTuning::kBattlefieldMaximumZ),
            static_cast<double>(battle->GetConfig().enemy.deploymentMinimumZ),
            static_cast<double>(battle->GetConfig().enemy.deploymentMaximumZ));
        list.Text(line, 16, 462);
    }
    std::snprintf(line, sizeof(line),
        "Player Impact: %s | Timer %.2f",
        ImpactStateName(movement.GetImpactState()),
        static_cast<double>(movement.GetImpactStateTimer()));
    list.Text(line, 16, 484);
    bool helmet = false;
    bool cockpit = false;
    for (const ModelInstance& model : scene.models) {
        helmet |= model.id == "fullgame_helmet";
        cockpit |= model.id == "fullgame_cockpit";
    }
    std::snprintf(line, sizeof(line), "First Person Model: %s | F2 Cycle",
        helmet && cockpit ? "Helmet + Cockpit" : helmet ? "Helmet" :
        cockpit ? "Cockpit" : "Hidden");
    list.Text(line, 16, 506);
    std::snprintf(line, sizeof(line),
        "Ultimate: %-8s Charge %3.0f%% | System %s",
        UltimateStateName(player.GetUltimateState()),
        static_cast<double>(player.GetUltimateChargeRatio() * 100.0f),
        player.IsSystemOffline() ? "OFFLINE" : "ONLINE");
    list.Text(line, 16, 528);
}
}

void BuildFullGameUi(const FlowScreenState& flow, const TutorialSession* tutorial,
    const CombatSession* battle, const FirstPersonGame& player,
    double elapsedSeconds, const CockpitPresentation& cockpit,
    bool combatDataVisible, const ModelScene& scene, UiDrawList& list,
    float damageVignetteAlpha, int outputWidth, int outputHeight) {
    list.Clear();
    if (flow.phase == FlowPhase::Title && flow.presentation != FlowPresentation::Story) {
        list.SetPlacement(UiPlacement::Cover);
        list.Image("title.png", 0, 0, 1280, 720);
        list.SetPlacement(UiPlacement::Safe);
        if (std::fmod(elapsedSeconds, 1.0) < 0.5)
            list.Image("startbar.png", 245, 520, 790, 170);
    } else if (flow.presentation == FlowPresentation::Story ||
        flow.presentation == FlowPresentation::EndingHold) {
        list.SetPlacement(UiPlacement::FullViewport);
        list.Rectangle(0, 0, 1280, 720, {0, 0, 0, 1});
        list.SetPlacement(UiPlacement::Safe);
        if (flow.page) {
            const float offset = flow.phase == FlowPhase::Prologue ? 200.0f : 0.0f;
            list.Image(flow.page->image, (1280.0f - flow.page->width) * 0.5f + offset,
                flow.pageY, flow.page->width, flow.page->height, {}, true);
            if (flow.page->mask) {
                list.SetPlacement(UiPlacement::FullWidth);
                list.Rectangle(0, 500, 1280, 220, {0, 0, 0, 1});
                list.SetPlacement(UiPlacement::Safe);
            }
        }
        if (flow.presentation == FlowPresentation::EndingHold) {
            list.Text("MISSION COMPLETE", 500, 650, 1.3f);
            list.Text("ENTER / A / K / START: RETURN TO TITLE", 380, 682, 1.0f);
        } else if (flow.showPrompt) {
            list.SetPlacement(UiPlacement::RightEdge);
            list.Text(flow.phase == FlowPhase::Prologue
                ? "SPACE / ENTER / A: FAST FORWARD"
                : "SPACE / ENTER / A: NEXT",
                flow.phase == FlowPhase::Prologue ? 987.0f : 1059.0f,
                650, 1.0f);
            list.Text("K / START: SKIP", 1095, 682, 1.0f);
            list.SetPlacement(UiPlacement::Safe);
        }
    } else if (flow.phase == FlowPhase::Tutorial || flow.phase == FlowPhase::Day1 ||
        flow.phase == FlowPhase::Day2 || flow.phase == FlowPhase::Boss) {
        if (flow.presentation == FlowPresentation::Opening &&
            cockpit.IsStartupActive()) {
            DrawCockpitOverlay(list, cockpit);
        }
        if (flow.presentation == FlowPresentation::Normal) {
            if (damageVignetteAlpha > 0.0f) {
                list.SetPlacement(UiPlacement::FullViewport);
                list.Rectangle(0, 0, 1280, 720,
                    {1.0f, 0.04f, 0.04f,
                        std::clamp(damageVignetteAlpha, 0.0f, 1.0f)});
                list.SetPlacement(UiPlacement::Safe);
            }
            list.SetPlacement(UiPlacement::RightEdge);
            list.Text("F4: SKIP PHASE", 1040, 18, 1.0f);
            list.SetPlacement(UiPlacement::Safe);
            if (cockpit.IsActive()) {
                DrawCockpitPresentation(list, cockpit);
                if (tutorial || cockpit.IsStartupActive())
                    DrawHud(list, player, battle, tutorial, cockpit, scene, false,
                        combatDataVisible, outputWidth, outputHeight);
            } else {
                DrawHud(list, player, battle, tutorial, cockpit, scene, true,
                    combatDataVisible, outputWidth, outputHeight);
            }
        }
        if (flow.presentation == FlowPresentation::Normal && !cockpit.IsActive() &&
            flow.phase == FlowPhase::Tutorial && tutorial &&
            (!tutorial->IsComplete() || tutorial->ShowFreePracticeCaption())) {
            list.Rectangle(348, 100, 560, 122, kPanel);
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
                std::snprintf(line, sizeof(line), "%s   L %s   R %s",
                    tutorial->GetStep() == TutorialStep::Punch
                        ? "SHIFT + LMB / RMB: PUNCH"
                        : "LMB / RMB: ROCKET PUNCH",
                    tutorial->HasLeftHit() ? "PASS" : "WAIT",
                    tutorial->HasRightHit() ? "PASS" : "WAIT");
                list.Text(line, 360, 168);
            } else {
                if (tutorial->IsComplete()) {
                    list.Text("SYSTEM ALL GREEN", 360, 168);
                    list.Text("FREE PRACTICE - ALL FUNCTIONS UNLOCKED", 360, 190);
                } else if (tutorial->GetStep() == TutorialStep::Defense) {
                    list.Text("INTERCEPT THE MISSILE OR LET THE SHIELD ABSORB IT",
                        360, 168);
                } else if (tutorial->GetStep() == TutorialStep::Dodge) {
                    list.Text("PRESS [SPACE] / [A] TO PERFORM A DASH", 360, 168);
                } else {
                    list.Text(tutorial->GetInstruction(), 360, 168);
                }
            }
        }
        if (flow.presentation == FlowPresentation::Normal && !cockpit.IsActive() &&
            flow.phase == FlowPhase::Tutorial && tutorial) {
            list.SetPlacement(UiPlacement::RightEdge);
            if (tutorial->ShowFreePracticeCaption())
                list.Text("ENTER / START: CONTINUE", 1020, 690, 1.0f);
            else if (!tutorial->IsComplete())
                list.Text("K / START: SKIP TUTORIAL", 1020, 690, 1.0f);
            list.SetPlacement(UiPlacement::Safe);
        }
        if (flow.presentation == FlowPresentation::Normal && !cockpit.IsActive() &&
            combatDataVisible) {
            DrawCombatData(list, battle, flow.phase, player, scene);
        }
    }
    if (flow.presentation == FlowPresentation::Closing ||
        flow.presentation == FlowPresentation::Opening) {
        list.SetPlacement(UiPlacement::FullViewport);
        list.Rectangle(0, -720.0f + 720.0f * flow.curtainAmount,
            1280, 720, {0, 0, 0, 1});
        list.SetPlacement(UiPlacement::Safe);
    }
}
