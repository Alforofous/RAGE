#include "vox_loader.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>

namespace RAGE::Content {
    namespace {
        constexpr uint32_t kMagicVox = 0x20584F56u;
        constexpr uint32_t kChunkMain = 0x4E49414Du;
        constexpr uint32_t kChunkSize = 0x455A4953u;
        constexpr uint32_t kChunkXyzi = 0x495A5958u;
        constexpr uint32_t kChunkRgba = 0x41424752u;
        constexpr uint32_t kMinVersion = 150u;
        constexpr size_t kHeaderBytes = 20;
        constexpr size_t kChunkHeaderBytes = 12;
        constexpr size_t kPaletteBytes = 1024;

        struct ParsedChunks {
            IVec3 dims{};
            bool haveSize = false;
            std::vector<std::array<uint8_t, 4>> rawVoxels;
            std::array<uint32_t, 256> filePalette{};
            bool havePalette = false;
        };

        uint32_t readU32(const uint8_t *p) {
            uint32_t v = 0;
            std::memcpy(&v, p, sizeof(v));
            return v;
        }

        int32_t readI32(const uint8_t *p) {
            return static_cast<int32_t>(readU32(p));
        }

        std::vector<uint8_t> readAll(const std::filesystem::path &path) {
            std::ifstream f(path, std::ios::binary | std::ios::ate);
            if (!f.is_open()) {
                throw std::runtime_error("vox_loader: failed to open file " + path.string());
            }
            const auto size = f.tellg();
            f.seekg(0);
            std::vector<uint8_t> bytes(static_cast<size_t>(size));
            if (size > 0) {
                f.read(reinterpret_cast<char *>(bytes.data()), size);
            }
            return bytes;
        }

        void parseSizeChunk(std::span<const uint8_t> content, ParsedChunks &out) {
            if (content.size() < 12) {
                throw std::runtime_error("vox_loader: SIZE chunk too short");
            }
            if (out.haveSize) {
                return;
            }
            out.dims.x = readI32(content.data());
            out.dims.y = readI32(content.data() + 4);
            out.dims.z = readI32(content.data() + 8);
            out.haveSize = true;
        }

        void parseXyziChunk(std::span<const uint8_t> content, ParsedChunks &out) {
            if (content.size() < 4) {
                throw std::runtime_error("vox_loader: XYZI chunk too short");
            }
            if (!out.rawVoxels.empty()) {
                return;
            }
            const uint32_t numVoxels = readU32(content.data());
            const size_t needed = static_cast<size_t>(4) + (static_cast<size_t>(numVoxels) * 4u);
            if (needed > content.size()) {
                throw std::runtime_error("vox_loader: XYZI count exceeds chunk size");
            }
            out.rawVoxels.resize(numVoxels);
            std::memcpy(out.rawVoxels.data(), content.data() + 4, static_cast<size_t>(numVoxels) * 4u);
        }

        void parseRgbaChunk(std::span<const uint8_t> content, ParsedChunks &out) {
            if (content.size() < kPaletteBytes) {
                throw std::runtime_error("vox_loader: RGBA chunk too short");
            }
            std::memcpy(out.filePalette.data(), content.data(), kPaletteBytes);
            out.havePalette = true;
        }

        ParsedChunks scanChunks(std::span<const uint8_t> region) {
            ParsedChunks out;
            size_t pos = 0;
            while (pos + kChunkHeaderBytes <= region.size()) {
                const uint32_t chunkId = readU32(region.data() + pos);
                const uint32_t contentSize = readU32(region.data() + pos + 4);
                const uint32_t childSize = readU32(region.data() + pos + 8);
                const size_t contentStart = pos + kChunkHeaderBytes;
                const size_t chunkEnd = contentStart + contentSize + childSize;
                if (chunkEnd > region.size()) {
                    throw std::runtime_error("vox_loader: chunk extends past parent");
                }

                const std::span<const uint8_t> content(region.data() + contentStart, contentSize);
                if (chunkId == kChunkSize) {
                    parseSizeChunk(content, out);
                } else if (chunkId == kChunkXyzi) {
                    parseXyziChunk(content, out);
                } else if (chunkId == kChunkRgba) {
                    parseRgbaChunk(content, out);
                }

                pos = chunkEnd;
            }
            return out;
        }

        size_t voxelIndex(IVec3 c, IVec3 dims) {
            return (static_cast<size_t>(c.z) * static_cast<size_t>(dims.x) * static_cast<size_t>(dims.y))
                   + (static_cast<size_t>(c.y) * static_cast<size_t>(dims.x)) + static_cast<size_t>(c.x);
        }
    }

    VoxModel loadVox(const std::filesystem::path &path) {
        const std::vector<uint8_t> bytes = readAll(path);
        if (bytes.size() < kHeaderBytes) {
            throw std::runtime_error("vox_loader: file too small to contain header");
        }

        if (readU32(bytes.data()) != kMagicVox) {
            throw std::runtime_error("vox_loader: bad magic (expected 'VOX ')");
        }

        const uint32_t version = readU32(bytes.data() + 4);
        if (version < kMinVersion) {
            throw std::runtime_error("vox_loader: unsupported version " + std::to_string(version));
        }

        if (readU32(bytes.data() + 8) != kChunkMain) {
            throw std::runtime_error("vox_loader: expected MAIN chunk after header");
        }

        const uint32_t mainContentSize = readU32(bytes.data() + 12);
        const uint32_t mainChildSize = readU32(bytes.data() + 16);
        const size_t mainChildStart = kHeaderBytes + mainContentSize;
        const size_t mainEnd = mainChildStart + mainChildSize;
        if (mainEnd > bytes.size()) {
            throw std::runtime_error("vox_loader: MAIN child region exceeds file size");
        }

        const ParsedChunks parsed = scanChunks(std::span<const uint8_t>(bytes.data() + mainChildStart, mainChildSize));

        if (!parsed.haveSize) {
            throw std::runtime_error("vox_loader: missing SIZE chunk");
        }
        if (parsed.dims.x <= 0 || parsed.dims.y <= 0 || parsed.dims.z <= 0) {
            throw std::runtime_error("vox_loader: SIZE dimensions must be positive");
        }

        VoxModel model;
        model.dims = parsed.dims;
        const size_t voxelCount = static_cast<size_t>(parsed.dims.x) * static_cast<size_t>(parsed.dims.y)
                                  * static_cast<size_t>(parsed.dims.z);
        model.voxels.assign(voxelCount, 0u);

        for (const auto &v : parsed.rawVoxels) {
            const IVec3 coord{ v[0], v[1], v[2] };
            const uint8_t colorIdx = v[3];
            if (coord.x < 0 || coord.x >= parsed.dims.x || coord.y < 0 || coord.y >= parsed.dims.y || coord.z < 0
                || coord.z >= parsed.dims.z) {
                continue;
            }
            uint32_t rgba = 0xFFFFFFFFu;
            if (parsed.havePalette && colorIdx != 0u) {
                rgba = parsed.filePalette[(colorIdx - 1u) & 0xFFu];
            }
            model.voxels[voxelIndex(coord, parsed.dims)] = rgba;
        }

        return model;
    }
}
