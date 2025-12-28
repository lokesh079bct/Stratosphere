#include "assets/MeshFormats.h"
#include <cstdio>
#include <cstring>

namespace Engine
{

    static bool fread_exact(FILE *f, void *dst, size_t bytes)
    {
        return std::fread(dst, 1, bytes, f) == bytes;
    }
    static size_t file_size(FILE *f)
    {
        long cur = std::ftell(f);
        std::fseek(f, 0, SEEK_END);
        long end = std::ftell(f);
        std::fseek(f, cur, SEEK_SET);
        return end >= 0 ? static_cast<size_t>(end) : 0;
    }

    bool LoadSMeshV0FromFile(const std::string &path, MeshData &out)
    {
        FILE *f = std::fopen(path.c_str(), "rb");
        if (!f)
            return false;

        SMeshHeaderV0 hdr{};
        if (!fread_exact(f, &hdr, sizeof(hdr)))
        {
            std::fclose(f);
            return false;
        }

        if (hdr.vertexStride != 32)
        {
            std::fclose(f);
            return false;
        }
        if (hdr.indexFormat > 1u)
        {
            std::fclose(f);
            return false;
        }

        size_t fsize = file_size(f);
        size_t vertexBytes = static_cast<size_t>(hdr.vertexCount) * hdr.vertexStride;
        size_t indexBytes = static_cast<size_t>(hdr.indexCount) * (hdr.indexFormat == 0 ? sizeof(uint16_t) : sizeof(uint32_t));
        if (hdr.vertexDataOffset + vertexBytes > fsize ||
            hdr.indexDataOffset + indexBytes > fsize)
        {
            std::fclose(f);
            return false;
        }

        out.vertexCount = hdr.vertexCount;
        out.indexCount = hdr.indexCount;
        out.vertexStride = hdr.vertexStride;
        out.indexFormat = hdr.indexFormat;
        std::memcpy(out.aabbMin, hdr.aabbMin, sizeof(out.aabbMin));
        std::memcpy(out.aabbMax, hdr.aabbMax, sizeof(out.aabbMax));

        out.vertexBytes.resize(vertexBytes);
        std::fseek(f, static_cast<long>(hdr.vertexDataOffset), SEEK_SET);
        if (!fread_exact(f, out.vertexBytes.data(), out.vertexBytes.size()))
        {
            std::fclose(f);
            return false;
        }

        if (hdr.indexFormat == 1)
        {
            out.indices32.resize(hdr.indexCount);
            std::fseek(f, static_cast<long>(hdr.indexDataOffset), SEEK_SET);
            if (!fread_exact(f, out.indices32.data(), out.indices32.size() * sizeof(uint32_t)))
            {
                std::fclose(f);
                return false;
            }
        }
        else
        {
            out.indices16.resize(hdr.indexCount);
            std::fseek(f, static_cast<long>(hdr.indexDataOffset), SEEK_SET);
            if (!fread_exact(f, out.indices16.data(), out.indices16.size() * sizeof(uint16_t)))
            {
                std::fclose(f);
                return false;
            }
        }

        std::fclose(f);
        return true;
    }

} // namespace Engine