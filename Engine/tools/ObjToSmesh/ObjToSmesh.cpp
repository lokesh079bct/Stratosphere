#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "assets/MeshFormats.h" // reuse SMeshHeaderV0

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <limits>
#include <cmath>

namespace fs = std::filesystem;

struct VertexPNUT
{
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

struct VertexKey
{
    int32_t vx, vy, vz; // quantized position
    int32_t nx, ny, nz; // quantized normal
    int32_t u, v;       // quantized uv
    bool operator==(const VertexKey &o) const
    {
        return vx == o.vx && vy == o.vy && vz == o.vz &&
               nx == o.nx && ny == o.ny && nz == o.nz &&
               u == o.u && v == o.v;
    }
};
struct VertexKeyHash
{
    std::size_t operator()(const VertexKey &k) const noexcept
    {
        uint64_t h = 1469598103934665603ull;
        auto mix = [&](int32_t v)
        { h ^= static_cast<uint64_t>(v); h *= 1099511628211ull; };
        mix(k.vx);
        mix(k.vy);
        mix(k.vz);
        mix(k.nx);
        mix(k.ny);
        mix(k.nz);
        mix(k.u);
        mix(k.v);
        return static_cast<std::size_t>(h);
    }
};

static bool writeBinary(const std::string &path, const std::vector<uint8_t> &bytes)
{
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    size_t wrote = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return wrote == bytes.size();
}

static void computeAABB(const std::vector<VertexPNUT> &verts, float minOut[3], float maxOut[3])
{
    minOut[0] = minOut[1] = minOut[2] = std::numeric_limits<float>::max();
    maxOut[0] = maxOut[1] = maxOut[2] = std::numeric_limits<float>::lowest();
    for (const auto &v : verts)
    {
        minOut[0] = std::min(minOut[0], v.px);
        minOut[1] = std::min(minOut[1], v.py);
        minOut[2] = std::min(minOut[2], v.pz);
        maxOut[0] = std::max(maxOut[0], v.px);
        maxOut[1] = std::max(maxOut[1], v.py);
        maxOut[2] = std::max(maxOut[2], v.pz);
    }
}

static int32_t qfloat(float x, float scale)
{
    return static_cast<int32_t>(std::round(x * scale));
}

static bool convertObjToSMesh(const fs::path &objPath, const fs::path &outPath)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    fs::path mtlBaseDir = objPath.parent_path();
    bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                               objPath.string().c_str(), mtlBaseDir.string().c_str(),
                               true /*triangulate*/);
    if (!warn.empty())
        std::cerr << "[tinyobj] warn: " << warn << "\n";
    if (!err.empty())
        std::cerr << "[tinyobj] err: " << err << "\n";
    if (!ok)
    {
        std::cerr << "Failed to load OBJ: " << objPath << "\n";
        return false;
    }

    std::unordered_map<VertexKey, uint32_t, VertexKeyHash> remap;
    std::vector<VertexPNUT> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(1024);
    indices.reserve(2048);

    const float QPOS = 100000.0f;
    const float QNORM = 10000.0f;
    const float QUV = 10000.0f;

    auto fetch = [&](int vidx, int nidx, int tidx) -> VertexPNUT
    {
        VertexPNUT v{};
        if (vidx >= 0)
        {
            v.px = attrib.vertices[3 * vidx + 0];
            v.py = attrib.vertices[3 * vidx + 1];
            v.pz = attrib.vertices[3 * vidx + 2];
        }
        if (nidx >= 0 && !attrib.normals.empty())
        {
            v.nx = attrib.normals[3 * nidx + 0];
            v.ny = attrib.normals[3 * nidx + 1];
            v.nz = attrib.normals[3 * nidx + 2];
        }
        else
        {
            v.nx = v.ny = 0.0f;
            v.nz = 1.0f;
        }
        if (tidx >= 0 && !attrib.texcoords.empty())
        {
            v.u = attrib.texcoords[2 * tidx + 0];
            v.v = attrib.texcoords[2 * tidx + 1];
        }
        else
        {
            v.u = v.v = 0.0f;
        }
        return v;
    };

    auto makeKey = [&](const VertexPNUT &v) -> VertexKey
    {
        VertexKey k;
        k.vx = qfloat(v.px, QPOS);
        k.vy = qfloat(v.py, QPOS);
        k.vz = qfloat(v.pz, QPOS);
        k.nx = qfloat(v.nx, QNORM);
        k.ny = qfloat(v.ny, QNORM);
        k.nz = qfloat(v.nz, QNORM);
        k.u = qfloat(v.u, QUV);
        k.v = qfloat(v.v, QUV);
        return k;
    };

    for (const auto &shape : shapes)
    {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
        {
            int fv = shape.mesh.num_face_vertices[f];
            if (fv != 3)
            {
                index_offset += fv;
                continue;
            }
            for (int vtx = 0; vtx < fv; vtx++)
            {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + vtx];
                VertexPNUT v = fetch(idx.vertex_index, idx.normal_index, idx.texcoord_index);
                VertexKey key = makeKey(v);

                auto it = remap.find(key);
                uint32_t vi;
                if (it == remap.end())
                {
                    vi = static_cast<uint32_t>(vertices.size());
                    remap.emplace(key, vi);
                    vertices.push_back(v);
                }
                else
                {
                    vi = it->second;
                }
                indices.push_back(vi);
            }
            index_offset += fv;
        }
    }

    if (vertices.empty() || indices.empty())
    {
        std::cerr << "OBJ contained no triangles: " << objPath << "\n";
        return false;
    }

    float aabbMin[3], aabbMax[3];
    computeAABB(vertices, aabbMin, aabbMax);

    Engine::SMeshHeaderV0 hdr{};
    hdr.vertexCount = static_cast<uint32_t>(vertices.size());
    hdr.indexCount = static_cast<uint32_t>(indices.size());
    hdr.vertexStride = sizeof(VertexPNUT); // 32
    hdr.indexFormat = 1;                   // uint32 for now
    hdr.aabbMin[0] = aabbMin[0];
    hdr.aabbMin[1] = aabbMin[1];
    hdr.aabbMin[2] = aabbMin[2];
    hdr.aabbMax[0] = aabbMax[0];
    hdr.aabbMax[1] = aabbMax[1];
    hdr.aabbMax[2] = aabbMax[2];

    const uint32_t headerSize = sizeof(Engine::SMeshHeaderV0);
    const uint32_t vertexBytes = hdr.vertexCount * hdr.vertexStride;
    const uint32_t indexBytes = hdr.indexCount * sizeof(uint32_t);
    hdr.vertexDataOffset = headerSize;
    hdr.indexDataOffset = headerSize + vertexBytes;

    std::vector<uint8_t> blob;
    blob.resize(headerSize + vertexBytes + indexBytes);
    std::memcpy(blob.data(), &hdr, sizeof(hdr));
    std::memcpy(blob.data() + hdr.vertexDataOffset, vertices.data(), vertexBytes);
    std::memcpy(blob.data() + hdr.indexDataOffset, indices.data(), indexBytes);

    fs::create_directories(outPath.parent_path());
    if (!writeBinary(outPath.string(), blob))
    {
        std::cerr << "Failed to write: " << outPath << "\n";
        return false;
    }
    std::cout << "Wrote " << outPath << " (verts=" << hdr.vertexCount << ", indices=" << hdr.indexCount << ")\n";
    return true;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: ObjToSMesh <input_obj_or_dir> <output_dir>\n";
        return 1;
    }
    fs::path input = argv[1];
    fs::path outDir = argv[2];
    std::error_code ec;
    fs::create_directories(outDir, ec);

    if (fs::is_regular_file(input) && input.extension() == ".obj")
    {
        fs::path outPath = outDir / (input.stem().string() + ".smesh");
        return convertObjToSMesh(input, outPath) ? 0 : 2;
    }
    else if (fs::is_directory(input))
    {
        int failures = 0;
        for (auto &p : fs::recursive_directory_iterator(input))
        {
            if (p.is_regular_file() && p.path().extension() == ".obj")
            {
                fs::path rel = fs::relative(p.path(), input);
                fs::path outPath = outDir / rel;
                outPath.replace_extension(".smesh");
                fs::create_directories(outPath.parent_path(), ec);
                if (!convertObjToSMesh(p.path(), outPath))
                    failures++;
            }
        }
        return failures == 0 ? 0 : 3;
    }
    else
    {
        std::cerr << "Input must be an .obj file or a directory containing .obj files\n";
        return 4;
    }
}