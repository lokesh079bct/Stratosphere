#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "assets/Handles.h" // MeshHandle, MaterialHandle
#include "assets/model/SModelAnimationRecords.h"

namespace Engine
{
    // ------------------------------------------------------------
    // ModelAsset (CPU-only)
    // ------------------------------------------------------------
    // A model = list of primitives (mesh + material + draw range).
    // Later, renderer will iterate primitives and draw them.
    struct ModelPrimitive
    {
        MeshHandle mesh{};
        MaterialHandle material{};

        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        int32_t vertexOffset = 0;
    };

    struct ModelAsset
    {
        struct AnimationState
        {
            uint32_t clipIndex = 0;
            float timeSec = 0.0f;
            float speed = 1.0f;
            bool loop = false;
            bool playing = false;
        };

        struct NodeTRS
        {
            glm::vec3 t{0.0f, 0.0f, 0.0f};
            glm::quat r{1.0f, 0.0f, 0.0f, 0.0f};
            glm::vec3 s{1.0f, 1.0f, 1.0f};
        };

        std::vector<ModelPrimitive> primitives;

        // Node graph
        struct ModelNode
        {
            uint32_t parentIndex{~0u};
            uint32_t firstChildIndex{~0u};
            uint32_t childCount{0};

            uint32_t firstPrimitiveIndex{0};
            uint32_t primitiveCount{0};

            glm::mat4 localMatrix{1.0f};
            glm::mat4 globalMatrix{1.0f};

            const char *debugName{nullptr};
        };

        std::vector<ModelNode> nodes;
        std::vector<uint32_t> nodePrimitiveIndices;
        std::vector<uint32_t> nodeChildIndices;
        uint32_t rootNodeIndex{0};

        // ------------------------------------------------------------
        // Animations (node TRS only, no skinning yet)
        // ------------------------------------------------------------
        AnimationState animState;

        std::vector<NodeTRS> restTRS;     // bind pose derived from localMatrix at load
        std::vector<NodeTRS> animatedTRS; // evaluated each frame

        std::vector<smodel::SModelAnimationClipRecord> animClips;
        std::vector<smodel::SModelAnimationChannelRecord> animChannels;
        std::vector<smodel::SModelAnimationSamplerRecord> animSamplers;
        std::vector<float> animTimes;
        std::vector<float> animValues;

        // Optional debug name (string table later)
        const char *debugName = "";

        // Aggregate bounds across all meshes used by the model
        float boundsMin[3]{0.0f, 0.0f, 0.0f};
        float boundsMax[3]{0.0f, 0.0f, 0.0f};
        bool hasBounds = false;

        // Precomputed center and uniform scale to fit target size (e.g., 20 units)
        float center[3]{0.0f, 0.0f, 0.0f};
        float fitScale = 1.0f;

        static inline glm::mat4 ComposeTRS(const NodeTRS &x)
        {
            glm::mat4 T = glm::translate(glm::mat4(1.0f), x.t);
            glm::mat4 R = glm::mat4_cast(glm::normalize(x.r));
            glm::mat4 S = glm::scale(glm::mat4(1.0f), x.s);
            return T * R * S;
        }

        static inline uint32_t FindKeyInterval(const float *times, uint32_t count, float t)
        {
            if (count <= 1)
                return 0;
            if (t <= times[0])
                return 0;
            if (t >= times[count - 2])
                return count - 2;

            uint32_t lo = 0, hi = count - 1;
            while (hi - lo > 1)
            {
                uint32_t mid = (lo + hi) / 2;
                if (times[mid] <= t)
                    lo = mid;
                else
                    hi = mid;
            }
            return lo;
        }

        static inline float ComputeAlpha(float t0, float t1, float t)
        {
            float dt = t1 - t0;
            if (dt <= 1e-8f)
                return 0.0f;
            float a = (t - t0) / dt;
            if (a < 0.0f)
                a = 0.0f;
            if (a > 1.0f)
                a = 1.0f;
            return a;
        }

        static inline glm::vec3 SampleVec3(const float *times, const float *values, uint32_t keyCount, float t)
        {
            if (keyCount == 0)
                return glm::vec3(0.0f);
            if (keyCount == 1)
                return glm::vec3(values[0], values[1], values[2]);

            uint32_t i = FindKeyInterval(times, keyCount, t);
            float t0 = times[i];
            float t1 = times[i + 1];
            float a = ComputeAlpha(t0, t1, t);

            const float *v0 = values + i * 3;
            const float *v1 = values + (i + 1) * 3;
            glm::vec3 p0(v0[0], v0[1], v0[2]);
            glm::vec3 p1(v1[0], v1[1], v1[2]);
            return glm::mix(p0, p1, a);
        }

        static inline glm::quat SampleQuat(const float *times, const float *values, uint32_t keyCount, float t)
        {
            if (keyCount == 0)
                return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            if (keyCount == 1)
            {
                const float *v = values;
                glm::quat q(v[3], v[0], v[1], v[2]); // stored XYZW
                return glm::normalize(q);
            }

            uint32_t i = FindKeyInterval(times, keyCount, t);
            float t0 = times[i];
            float t1 = times[i + 1];
            float a = ComputeAlpha(t0, t1, t);

            const float *v0 = values + i * 4;
            const float *v1 = values + (i + 1) * 4;

            glm::quat q0(v0[3], v0[0], v0[1], v0[2]);
            glm::quat q1(v1[3], v1[0], v1[1], v1[2]);

            q0 = glm::normalize(q0);
            q1 = glm::normalize(q1);

            if (glm::dot(q0, q1) < 0.0f)
                q1 = -q1;

            return glm::normalize(glm::slerp(q0, q1, a));
        }

        inline void recomputeGlobals()
        {
            const uint32_t nodeCount = static_cast<uint32_t>(nodes.size());
            if (nodeCount == 0)
                return;

            const uint32_t U32_MAX = ~0u;
            std::vector<uint8_t> visited(nodeCount, 0);

            std::function<void(uint32_t, const glm::mat4 &)> compute = [&](uint32_t nodeIdx, const glm::mat4 &parentGlobal)
            {
                if (nodeIdx >= nodeCount)
                    return;
                if (visited[nodeIdx])
                    return;
                visited[nodeIdx] = 1;

                ModelNode &n = nodes[nodeIdx];
                n.globalMatrix = parentGlobal * n.localMatrix;

                if (n.childCount == 0)
                    return;
                if (n.firstChildIndex == U32_MAX)
                    return;

                const uint32_t start = n.firstChildIndex;
                for (uint32_t ci = 0; ci < n.childCount; ++ci)
                {
                    const uint32_t child = nodeChildIndices[start + ci];
                    compute(child, n.globalMatrix);
                }
            };

            for (uint32_t i = 0; i < nodeCount; ++i)
            {
                if (nodes[i].parentIndex == U32_MAX)
                    compute(i, glm::mat4(1.0f));
            }
        }

        inline void updateAnimation(float dtSeconds)
        {
            if (animClips.empty() || animChannels.empty() || animSamplers.empty())
                return;
            if (!animState.playing)
                return;
            if (nodes.empty())
                return;

            const uint32_t clipIndex = std::min(animState.clipIndex, static_cast<uint32_t>(animClips.size() - 1));
            const auto &clip = animClips[clipIndex];

            const float duration = clip.durationSec;
            if (duration <= 1e-6f)
                return;

            animState.timeSec += dtSeconds * animState.speed;
            if (animState.loop)
            {
                animState.timeSec = std::fmod(animState.timeSec, duration);
                if (animState.timeSec < 0.0f)
                    animState.timeSec += duration;
            }
            else
            {
                if (animState.timeSec < 0.0f)
                    animState.timeSec = 0.0f;
                if (animState.timeSec > duration)
                    animState.timeSec = duration;
            }

            const float t = animState.timeSec;

            if (restTRS.size() == nodes.size())
            {
                animatedTRS = restTRS;
            }
            else
            {
                // Fallback: if restTRS not initialized, keep identity TRS.
                animatedTRS.assign(nodes.size(), NodeTRS{});
            }

            const uint32_t clipFirst = clip.firstChannel;
            const uint32_t clipCount = clip.channelCount;
            for (uint32_t ci = 0; ci < clipCount; ci++)
            {
                const uint32_t chIdx = clipFirst + ci;
                if (chIdx >= animChannels.size())
                    break;

                const auto &ch = animChannels[chIdx];
                if (ch.samplerIndex >= animSamplers.size())
                    continue;
                const auto &s = animSamplers[ch.samplerIndex];

                if (ch.targetNode >= animatedTRS.size())
                    continue;

                if (s.timeCount == 0)
                    continue;
                if (s.firstTime + s.timeCount > animTimes.size())
                    continue;

                const float *times = animTimes.data() + s.firstTime;
                const float *values = nullptr;
                if (s.firstValue + s.valueCount <= animValues.size())
                    values = animValues.data() + s.firstValue;
                else
                    continue;

                if (ch.path == (uint16_t)smodel::SModelAnimPath::Translation)
                {
                    animatedTRS[ch.targetNode].t = SampleVec3(times, values, s.timeCount, t);
                }
                else if (ch.path == (uint16_t)smodel::SModelAnimPath::Scale)
                {
                    animatedTRS[ch.targetNode].s = SampleVec3(times, values, s.timeCount, t);
                }
                else if (ch.path == (uint16_t)smodel::SModelAnimPath::Rotation)
                {
                    animatedTRS[ch.targetNode].r = SampleQuat(times, values, s.timeCount, t);
                }
            }

            for (size_t i = 0; i < nodes.size(); i++)
            {
                nodes[i].localMatrix = ComposeTRS(animatedTRS[i]);
            }

            recomputeGlobals();
        }

        // Evaluate clip at an explicit time into globalsOut (nodeCount matrices).
        // This does not mutate nodes/local/global matrices, so it can be used per entity.
        inline void evaluatePoseInto(uint32_t clipIndex, float timeSec,
                                     std::vector<NodeTRS> &trsScratch,
                                     std::vector<glm::mat4> &localsScratch,
                                     std::vector<glm::mat4> &globalsOut,
                                     std::vector<uint8_t> &visitedScratch) const
        {
            const uint32_t nodeCount = static_cast<uint32_t>(nodes.size());
            globalsOut.assign(nodeCount, glm::mat4(1.0f));
            if (nodeCount == 0)
                return;

            trsScratch.resize(nodeCount);
            localsScratch.resize(nodeCount);
            visitedScratch.assign(nodeCount, 0);

            // Start from rest pose if available.
            if (restTRS.size() == nodes.size())
            {
                trsScratch = restTRS;
            }
            else
            {
                for (uint32_t i = 0; i < nodeCount; ++i)
                    trsScratch[i] = NodeTRS{};
            }

            if (!animClips.empty() && !animChannels.empty() && !animSamplers.empty())
            {
                const uint32_t safeClip = std::min(clipIndex, static_cast<uint32_t>(animClips.size() - 1));
                const auto &clip = animClips[safeClip];

                const float t = timeSec;
                const uint32_t clipFirst = clip.firstChannel;
                const uint32_t clipCount = clip.channelCount;
                for (uint32_t ci = 0; ci < clipCount; ci++)
                {
                    const uint32_t chIdx = clipFirst + ci;
                    if (chIdx >= animChannels.size())
                        break;

                    const auto &ch = animChannels[chIdx];
                    if (ch.samplerIndex >= animSamplers.size())
                        continue;
                    const auto &s = animSamplers[ch.samplerIndex];

                    if (ch.targetNode >= trsScratch.size())
                        continue;

                    if (s.timeCount == 0)
                        continue;
                    if (s.firstTime + s.timeCount > animTimes.size())
                        continue;

                    const float *times = animTimes.data() + s.firstTime;
                    const float *values = nullptr;
                    if (s.firstValue + s.valueCount <= animValues.size())
                        values = animValues.data() + s.firstValue;
                    else
                        continue;

                    if (ch.path == (uint16_t)smodel::SModelAnimPath::Translation)
                    {
                        trsScratch[ch.targetNode].t = SampleVec3(times, values, s.timeCount, t);
                    }
                    else if (ch.path == (uint16_t)smodel::SModelAnimPath::Scale)
                    {
                        trsScratch[ch.targetNode].s = SampleVec3(times, values, s.timeCount, t);
                    }
                    else if (ch.path == (uint16_t)smodel::SModelAnimPath::Rotation)
                    {
                        trsScratch[ch.targetNode].r = SampleQuat(times, values, s.timeCount, t);
                    }
                }
            }

            for (uint32_t i = 0; i < nodeCount; ++i)
                localsScratch[i] = ComposeTRS(trsScratch[i]);

            const uint32_t U32_MAX = ~0u;

            std::function<void(uint32_t, const glm::mat4 &)> compute = [&](uint32_t nodeIdx, const glm::mat4 &parentGlobal)
            {
                if (nodeIdx >= nodeCount)
                    return;
                if (visitedScratch[nodeIdx])
                    return;
                visitedScratch[nodeIdx] = 1;

                globalsOut[nodeIdx] = parentGlobal * localsScratch[nodeIdx];

                const ModelNode &n = nodes[nodeIdx];
                if (n.childCount == 0)
                    return;
                if (n.firstChildIndex == U32_MAX)
                    return;

                const uint32_t start = n.firstChildIndex;
                for (uint32_t ci = 0; ci < n.childCount; ++ci)
                {
                    const uint32_t child = nodeChildIndices[start + ci];
                    compute(child, globalsOut[nodeIdx]);
                }
            };

            for (uint32_t i = 0; i < nodeCount; ++i)
            {
                if (nodes[i].parentIndex == U32_MAX)
                    compute(i, glm::mat4(1.0f));
            }
        }
    };

} // namespace Engine
