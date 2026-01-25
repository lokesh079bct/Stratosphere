#include "assets/SModelLoader.h"

#include <fstream>
#include <sstream>
#include <cstring> // std::memcpy
#include <limits>

namespace Engine
{
    namespace smodel
    {
        // ------------------------------------------------------------
        // Local helpers
        // ------------------------------------------------------------

        static bool isRangeInsideFile(uint64_t begin, uint64_t size, uint64_t fileSize)
        {
            // Prevent overflow
            if (begin > fileSize)
                return false;
            if (size > fileSize)
                return false;
            if (begin + size > fileSize)
                return false;
            return true;
        }

        template <typename T>
        static bool tableRangeValid(uint64_t tableOffset, uint64_t count, uint64_t fileSize, std::string &outError)
        {
            const uint64_t bytes = count * sizeof(T);
            if (!isRangeInsideFile(tableOffset, bytes, fileSize))
            {
                std::ostringstream oss;
                oss << "Table out of file bounds. offset=" << tableOffset
                    << " bytes=" << bytes << " fileSize=" << fileSize;
                outError = oss.str();
                return false;
            }
            return true;
        }

        // ------------------------------------------------------------
        // SModelFileView helper
        // ------------------------------------------------------------

        const char *SModelFileView::getStringOrEmpty(uint32_t strOffset) const
        {
            if (!header || !stringTable)
                return "";
            if (strOffset == 0)
                return "";

            // strOffset is relative to stringTableOffset, bounded by stringTableSize
            if (strOffset >= header->stringTableSize)
                return "";

            const char *s = stringTable + strOffset;

            // Ensure it is null-terminated within string table bounds
            const uint64_t maxRemaining = header->stringTableSize - strOffset;
            const void *nullPos = std::memchr(s, '\0', (size_t)maxRemaining);
            if (!nullPos)
                return "";

            return s;
        }

        // ------------------------------------------------------------
        // LoadSModelFile
        // ------------------------------------------------------------

        bool LoadSModelFile(const std::string &path, SModelFileView &outView, std::string &outError)
        {
            outError.clear();
            outView = SModelFileView{}; // reset

            // --------------------------
            // Read file bytes
            // --------------------------
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file.is_open())
            {
                outError = "Failed to open file: " + path;
                return false;
            }

            const std::streamsize fileSize = file.tellg();
            if (fileSize <= 0)
            {
                outError = "File is empty: " + path;
                return false;
            }

            file.seekg(0, std::ios::beg);

            outView.fileBytes.resize(static_cast<size_t>(fileSize));
            if (!file.read(reinterpret_cast<char *>(outView.fileBytes.data()), fileSize))
            {
                outError = "Failed to read file bytes: " + path;
                return false;
            }

            const uint64_t uFileSize = static_cast<uint64_t>(outView.fileBytes.size());
            if (uFileSize < sizeof(SModelHeader))
            {
                outError = "File too small to contain SModelHeader.";
                return false;
            }

            // --------------------------
            // Interpret header
            // --------------------------
            outView.header = reinterpret_cast<const SModelHeader *>(outView.fileBytes.data());

            // Basic compatibility
            if (!isHeaderCompatible(*outView.header))
            {
                outError = "SModel header incompatible (bad magic or unsupported version).";
                return false;
            }

            // Extra sanity check: header fileSizeBytes should match actual file size
            if (outView.header->fileSizeBytes != 0 && outView.header->fileSizeBytes != uFileSize)
            {
                // Not fatal, but helps detect broken cooking.
                // We enforce it strictly because this is a cooked file format.
                outError = "SModel header fileSizeBytes does not match actual file size.";
                return false;
            }

            // --------------------------
            // Validate section bounds
            // --------------------------

            // String table
            if (!isRangeInsideFile(outView.header->stringTableOffset, outView.header->stringTableSize, uFileSize))
            {
                outError = "String table out of bounds.";
                return false;
            }

            // Blob section
            if (!isRangeInsideFile(outView.header->blobOffset, outView.header->blobSize, uFileSize))
            {
                outError = "Blob section out of bounds.";
                return false;
            }

            // Record tables
            if (!tableRangeValid<SModelMeshRecord>(outView.header->meshesOffset, outView.header->meshCount, uFileSize, outError))
                return false;

            if (!tableRangeValid<SModelPrimitiveRecord>(outView.header->primitivesOffset, outView.header->primitiveCount, uFileSize, outError))
                return false;

            if (!tableRangeValid<SModelMaterialRecord>(outView.header->materialsOffset, outView.header->materialCount, uFileSize, outError))
                return false;

            if (!tableRangeValid<SModelTextureRecord>(outView.header->texturesOffset, outView.header->textureCount, uFileSize, outError))
                return false;

            // V2: nodes table
            if (outView.header->nodeCount > 0)
            {
                if (!tableRangeValid<SModelNodeRecord>(outView.header->nodesOffset, outView.header->nodeCount, uFileSize, outError))
                    return false;
            }

            // V2: nodePrimitiveIndices table (u32 entries)
            if (outView.header->nodePrimitiveIndexCount > 0)
            {
                const uint64_t bytes = uint64_t(outView.header->nodePrimitiveIndexCount) * sizeof(uint32_t);
                if (!isRangeInsideFile(outView.header->nodePrimitiveIndicesOffset, bytes, uFileSize))
                {
                    outError = "NodePrimitiveIndices table out of file bounds.";
                    return false;
                }
            }

            // --------------------------
            // Build pointers/views
            // --------------------------
            const uint8_t *base = outView.fileBytes.data();

            outView.meshes = reinterpret_cast<const SModelMeshRecord *>(base + outView.header->meshesOffset);
            outView.primitives = reinterpret_cast<const SModelPrimitiveRecord *>(base + outView.header->primitivesOffset);
            outView.materials = reinterpret_cast<const SModelMaterialRecord *>(base + outView.header->materialsOffset);
            outView.textures = reinterpret_cast<const SModelTextureRecord *>(base + outView.header->texturesOffset);

            // V2: nodes + indices
            if (outView.header->nodeCount > 0)
            {
                outView.nodes = reinterpret_cast<const SModelNodeRecord *>(base + outView.header->nodesOffset);
            }
            if (outView.header->nodePrimitiveIndexCount > 0)
            {
                outView.nodePrimitiveIndices = reinterpret_cast<const uint32_t *>(base + outView.header->nodePrimitiveIndicesOffset);
            }

            outView.stringTable = reinterpret_cast<const char *>(base + outView.header->stringTableOffset);
            outView.blob = reinterpret_cast<const uint8_t *>(base + outView.header->blobOffset);

            // --------------------------
            // Validate record internal offsets (blob offsets)
            // --------------------------
            // This is important because a record might point outside blob even if tables are valid.

            const uint64_t blobSize = outView.header->blobSize;

            // Validate mesh VB/IB slices
            for (uint32_t i = 0; i < outView.header->meshCount; i++)
            {
                const SModelMeshRecord &m = outView.meshes[i];

                if (m.vertexDataOffset + m.vertexDataSize > blobSize)
                {
                    outError = "Mesh vertex data slice out of blob bounds (meshIndex=" + std::to_string(i) + ")";
                    return false;
                }

                if (m.indexDataOffset + m.indexDataSize > blobSize)
                {
                    outError = "Mesh index data slice out of blob bounds (meshIndex=" + std::to_string(i) + ")";
                    return false;
                }

                // Optional: sanity checks
                if (m.vertexCount == 0 || m.vertexStride == 0)
                {
                    outError = "Mesh has invalid vertexCount/vertexStride (meshIndex=" + std::to_string(i) + ")";
                    return false;
                }

                // Ensure vertex blob size matches count*stride if cooked in that way.
                // Not strictly required, but good for catching tool mistakes.
                const uint64_t expectedVBSize = uint64_t(m.vertexCount) * uint64_t(m.vertexStride);
                if (m.vertexDataSize != expectedVBSize)
                {
                    outError = "Mesh vertexDataSize mismatch (meshIndex=" + std::to_string(i) + ")";
                    return false;
                }
            }

            // Validate texture image slices
            for (uint32_t i = 0; i < outView.header->textureCount; i++)
            {
                const SModelTextureRecord &t = outView.textures[i];

                if (t.imageDataOffset + t.imageDataSize > blobSize)
                {
                    outError = "Texture image data slice out of blob bounds (textureIndex=" + std::to_string(i) + ")";
                    return false;
                }
            }

            // Validate primitive references
            for (uint32_t i = 0; i < outView.header->primitiveCount; i++)
            {
                const SModelPrimitiveRecord &p = outView.primitives[i];

                if (p.meshIndex >= outView.header->meshCount)
                {
                    outError = "Primitive references invalid meshIndex (primitiveIndex=" + std::to_string(i) + ")";
                    return false;
                }

                if (p.materialIndex >= outView.header->materialCount)
                {
                    outError = "Primitive references invalid materialIndex (primitiveIndex=" + std::to_string(i) + ")";
                    return false;
                }

                // If indexCount is 0 in cooked data, we can treat it as "draw full mesh later".
                // But having 0 is usually not intended; keep it allowed for flexibility.
            }

            // Validate material texture indices
            for (uint32_t i = 0; i < outView.header->materialCount; i++)
            {
                const SModelMaterialRecord &mat = outView.materials[i];

                auto checkTex = [&](int32_t texIndex, const char *fieldName) -> bool
                {
                    if (texIndex < 0)
                        return true; // -1 = none
                    if (uint32_t(texIndex) >= outView.header->textureCount)
                    {
                        outError = std::string("Material references invalid texture index (materialIndex=") + std::to_string(i) + ", field=" + fieldName + ")";
                        return false;
                    }
                    return true;
                };

                if (!checkTex(mat.baseColorTexture, "baseColorTexture"))
                    return false;
                if (!checkTex(mat.normalTexture, "normalTexture"))
                    return false;
                if (!checkTex(mat.metallicRoughnessTexture, "metallicRoughnessTexture"))
                    return false;
                if (!checkTex(mat.occlusionTexture, "occlusionTexture"))
                    return false;
                if (!checkTex(mat.emissiveTexture, "emissiveTexture"))
                    return false;
            }

            // V2: validate node graph if present
            if (outView.header->nodeCount > 0)
            {
                const uint32_t nodeCount = outView.header->nodeCount;
                const uint32_t idxCount = outView.header->nodePrimitiveIndexCount;
                const uint32_t primCount = outView.header->primitiveCount;

                for (uint32_t ni = 0; ni < nodeCount; ++ni)
                {
                    const SModelNodeRecord &nr = outView.nodes[ni];

                    const uint32_t U32_MAX = std::numeric_limits<uint32_t>::max();
                    if (nr.parentIndex != U32_MAX && nr.parentIndex >= nodeCount)
                    {
                        outError = "Node parentIndex out of bounds";
                        return false;
                    }
                    if (nr.childCount > 0)
                    {
                        if (nr.firstChild == U32_MAX)
                        {
                            outError = "Node has children but firstChild == UINT32_MAX";
                            return false;
                        }
                        if (nr.firstChild + nr.childCount > nodeCount)
                        {
                            outError = "Node children range out of bounds";
                            return false;
                        }
                    }

                    if (nr.primitiveCount > 0)
                    {
                        if (nr.firstPrimitiveIndex + nr.primitiveCount > idxCount)
                        {
                            outError = "Node primitive index range out of bounds";
                            return false;
                        }
                        for (uint32_t k = 0; k < nr.primitiveCount; ++k)
                        {
                            const uint32_t pidx = outView.nodePrimitiveIndices[nr.firstPrimitiveIndex + k];
                            if (pidx >= primCount)
                            {
                                outError = "Node references invalid primitive index";
                                return false;
                            }
                        }
                    }
                }
            }

            // If we reach here, file is valid and view is ready.
            return true;
        }

    } // namespace smodel
} // namespace Engine
