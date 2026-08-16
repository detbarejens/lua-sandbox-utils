#include "AimBot.hpp"
#include "../../FiveM-External.hpp"
#include "../../Definations/Variables.hpp"
#include "../../Utils/VisibilityCheck.hpp"
#include <map>

namespace Cheat
{
    namespace
    {
        Vector3D GetTargetBonePosition(const Entity& entity, int hitbox)
        {
            switch (hitbox)
            {
            case 0: return g_Fivem.GetBonePosVec3(entity, SKEL_Head); // Changed from FB_Brow_Centre_000 to SKEL_Head
            case 1: return g_Fivem.GetBonePosVec3(entity, SKEL_Neck_1);
            case 2: return g_Fivem.GetBonePosVec3(entity, SKEL_Spine3);
            default: return {};
            }
        }
    }

    void AimBot::RunThread()
    {
        while (!g_Options.General.ShutDown)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            if (!g_Fivem.IsInitialized())
                continue;

            if (!g_Options.LegitBot.AimBot.Enabled)
                continue;

            if (!SafeCall(GetAsyncKeyState)(g_Options.LegitBot.AimBot.KeyBind))
                continue;

            const auto& localPlayer = g_Fivem.GetLocalPlayerInfo();
            if (!localPlayer.Ped)
                continue;

            Entity closestEntity{};
            bool found = g_Fivem.FindClosestEntity(
                g_Options.LegitBot.AimBot.FOV,
                g_Options.LegitBot.AimBot.MaxDistance,
                g_Options.LegitBot.AimBot.VisibleCheck,
                g_Options.LegitBot.AimBot.TargetNPC,
                g_Options.LegitBot.AimBot.ClosestFov,
                &closestEntity
            );

            if (!found || !closestEntity.StaticInfo.Ped)
                continue;

            if (closestEntity.StaticInfo.IsFriend)
                continue;

            if (g_Options.LegitBot.AimBot.VisibleCheck)
            {
                if (!FrameWork::Visibility::IsPlayerVisible(closestEntity, g_Options.Visuals.Players.VisiblePixelThreshold))
                    continue;
            }

            Vector3D targetPos = GetTargetBonePosition(closestEntity, g_Options.LegitBot.AimBot.HitBox);
            if (targetPos.IsZero())
                continue;

            // Prediction - predict target movement
            if (g_Options.LegitBot.AimBot.Prediction)
            {
                Vector3D currentPos = closestEntity.StaticInfo.Ped->GetCoordinate();
                static std::map<uint64_t, Vector3D> lastPositions;
                
                auto it = lastPositions.find((uint64_t)closestEntity.StaticInfo.Ped);
                if (it != lastPositions.end())
                {
                    // Calculate movement manually without operator-
                    Vector3D movement;
                    movement.x = currentPos.x - it->second.x;
                    movement.y = currentPos.y - it->second.y;
                    movement.z = currentPos.z - it->second.z;
                    
                    float predictionMult = g_Options.LegitBot.AimBot.PredictionMultiplier / 100.f;
                    targetPos = targetPos + (movement * predictionMult);
                }
                lastPositions[(uint64_t)closestEntity.StaticInfo.Ped] = currentPos;
            }

            // Check if player is aiming (RMB pressed)
            bool isAiming = (SafeCall(GetAsyncKeyState)(VK_RBUTTON) & 0x8000) != 0;
            
            // Calculate smooth values
            int smoothX = g_Options.LegitBot.AimBot.SmoothHorizontal;
            int smoothY = g_Options.LegitBot.AimBot.SmoothVertical;
            
            // If aiming, increase smooth to avoid conflict with game's aim control
            if (isAiming)
            {
                smoothX = (int)(smoothX * 1.5f); // 50% more smooth when aiming
                smoothY = (int)(smoothY * 1.5f);
            }

            // Aim Assist - subtle aim correction
            if (g_Options.LegitBot.AimBot.AimAssist)
            {
                float assistStrength = g_Options.LegitBot.AimBot.AimAssistStrength / 100.f;
                smoothX = (int)(smoothX * (1.f - assistStrength * 0.5f));
                smoothY = (int)(smoothY * (1.f - assistStrength * 0.5f));
                g_Fivem.ProcessCameraMovement(targetPos, smoothX, smoothY);
            }
            // Sticky Aim - aim "sticks" to target
            else if (g_Options.LegitBot.AimBot.StickyAim)
            {
                float stickyStrength = g_Options.LegitBot.AimBot.StickyAimStrength / 100.f;
                smoothX = (int)(smoothX * (1.f - stickyStrength * 0.7f));
                smoothY = (int)(smoothY * (1.f - stickyStrength * 0.7f));
                g_Fivem.ProcessCameraMovement(targetPos, smoothX, smoothY);
            }
            else
            {
                g_Fivem.ProcessCameraMovement(targetPos, smoothX, smoothY);
            }
        }
    }
}
