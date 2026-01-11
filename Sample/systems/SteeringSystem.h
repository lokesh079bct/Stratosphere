#pragma once
#include "ECS/SystemFormat.h"
#include "ECS/Components.h"
#include <algorithm>
#include <cmath>
#include <iostream>

class SteeringSystem : public Engine::ECS::SystemBase
{
public:
    SteeringSystem()
    {
        // Position + Velocity + MoveTarget + MoveSpeed required
        setRequiredNames({"Position", "Velocity", "MoveTarget", "MoveSpeed"});
        setExcludedNames({"Disabled", "Dead"});
    }

    const char *name() const override { return "SteeringSystem"; }

    void update(Engine::ECS::ArchetypeStoreManager &mgr, float dt) override
    {
        auto clamp = [](float v, float a, float b)
        { return std::max(a, std::min(v, b)); };
        auto len2 = [](float x, float z)
        { return std::sqrt(x * x + z * z); };

        // Gameplay uses meters; stop once we're within a small radius.
        const float arrivalRadius = 0.25f;
        // Snappy stop: no long slowdown phase.
        // We'll keep full speed, but clamp the final step so we hit the arrival radius cleanly.

        for (const auto &ptr : mgr.stores())
        {
            if (!ptr)
                continue;
            auto &store = *ptr;

            if (!store.signature().containsAll(required()))
                continue;
            if (!store.signature().containsNone(excluded()))
                continue;

            // Accessors: positions, velocities, moveTargets, moveSpeeds
            auto &positions = const_cast<std::vector<Engine::ECS::Position> &>(store.positions());
            auto &velocities = const_cast<std::vector<Engine::ECS::Velocity> &>(store.velocities());

            // You’ll add these to ArchetypeStore once you wire MoveTarget/MoveSpeed there:
            auto &targets = const_cast<std::vector<Engine::ECS::MoveTarget> &>(store.moveTargets());
            auto &speeds = const_cast<std::vector<Engine::ECS::MoveSpeed> &>(store.moveSpeeds());

            const auto &masks = store.rowMasks();
            const uint32_t n = store.size();

            for (uint32_t i = 0; i < n; ++i)
            {
                if (!masks[i].matches(required(), excluded()))
                    continue;

                auto &pos = positions[i];
                auto &vel = velocities[i];
                auto &tgt = targets[i];
                const auto &spd = speeds[i];

                if (!tgt.active)
                    continue;

                float dx = tgt.x - pos.x;
                float dz = tgt.z - pos.z;
                float dist = len2(dx, dz);

                if (dist <= arrivalRadius)
                {
                    vel.x = vel.y = vel.z = 0.0f;
                    tgt.active = 0;
                    continue;
                }

                // normalize
                if (dist > 1e-6f)
                {
                    dx /= dist;
                    dz /= dist;
                }
                else
                {
                    dx = dz = 0.0f;
                }

                // Snappy stop: constant speed, but avoid "creeping" by clamping so we reach the
                // arrival radius in this frame (then the next update will stop & clear the target).
                float desiredSpeed = spd.value;
                if (dt > 1e-6f)
                {
                    const float remaining = std::max(0.0f, dist - arrivalRadius);
                    const float maxSpeedThisFrame = remaining / dt;
                    desiredSpeed = std::min(desiredSpeed, maxSpeedThisFrame);
                }

                vel.x = dx * desiredSpeed;
                vel.z = dz * desiredSpeed;
                // Height axis is y; gameplay movement stays on the ground plane for now.
                vel.y = 0.0f;
            }
        }
    }
};