#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace Engine
{

    // Minimal header for .smesh (no magic/version)
    struct SMeshHeaderV0
    {
        uint32_t vertexCount;
        uint32_t indexCount;
        uint32_t vertexStride; // v0: 32 (pos3, norm3, uv2)
        uint32_t indexFormat;  // 0=uint16, 1=uint32
        float aabbMin[3];
        float aabbMax[3];
        uint32_t vertexDataOffset;
        uint32_t indexDataOffset;
    };

    struct MeshData
    {
        std::vector<uint8_t> vertexBytes; // size = vertexCount * vertexStride
        std::vector<uint32_t> indices32;  // used when indexFormat==1
        std::vector<uint16_t> indices16;  // used when indexFormat==0
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        uint32_t vertexStride = 0;
        uint32_t indexFormat = 1;
        float aabbMin[3]{};
        float aabbMax[3]{};
    };

    // Returns true on success, fills MeshData
    bool LoadSMeshV0FromFile(const std::string &path, MeshData &out);

} // namespace Engine