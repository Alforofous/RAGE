#include "file_chunk_store.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "engine/scene/voxel3d.hpp"

namespace RAGE::Content {
    namespace {
        constexpr uint32_t kMagic = 0x31434752u;
        constexpr uint32_t kFlagEmpty = 1u;
        constexpr size_t kHeaderBytes = (sizeof(uint32_t) * 2) + (sizeof(int32_t) * 3);

        struct Header {
            uint32_t magic = 0;
            uint32_t flags = 0;
            IVec3 voxelDims{};
        };

        size_t voxelCount(IVec3 dims) {
            return static_cast<size_t>(dims.x) * static_cast<size_t>(dims.y)
                   * static_cast<size_t>(dims.z);
        }

        void appendU32(std::vector<char> &out, uint32_t v) {
            const size_t at = out.size();
            out.resize(at + sizeof(v));
            std::memcpy(out.data() + at, &v, sizeof(v));
        }

        uint32_t readU32(const std::vector<char> &in, size_t &cursor, const std::string &path) {
            if (cursor + sizeof(uint32_t) > in.size()) {
                throw std::runtime_error("FileChunkStore: truncated chunk file: " + path);
            }
            uint32_t v = 0;
            std::memcpy(&v, in.data() + cursor, sizeof(v));
            cursor += sizeof(v);
            return v;
        }

        /// x-fastest linear order, matching `fillFromPackedRGBA8`'s expected layout.
        std::vector<uint32_t> snapshotVoxels(const Voxel3D &chunk) {
            const IVec3 dims = chunk.dimensions();
            std::vector<uint32_t> out;
            out.reserve(voxelCount(dims));
            const VoxelData *data = chunk.voxelData();
            for (int32_t z = 0; z < dims.z; ++z) {
                for (int32_t y = 0; y < dims.y; ++y) {
                    for (int32_t x = 0; x < dims.x; ++x) {
                        out.push_back(data->voxel(IVec3{ x, y, z }));
                    }
                }
            }
            return out;
        }

        std::vector<char> encode(const std::vector<uint32_t> &voxels, IVec3 dims) {
            bool allEmpty = true;
            for (const uint32_t v : voxels) {
                if (v != 0u) {
                    allEmpty = false;
                    break;
                }
            }

            std::vector<char> out;
            appendU32(out, kMagic);
            appendU32(out, allEmpty ? kFlagEmpty : 0u);
            appendU32(out, static_cast<uint32_t>(dims.x));
            appendU32(out, static_cast<uint32_t>(dims.y));
            appendU32(out, static_cast<uint32_t>(dims.z));
            if (allEmpty) {
                return out;
            }

            size_t runStart = 0;
            for (size_t i = 1; i <= voxels.size(); ++i) {
                if (i == voxels.size() || voxels[i] != voxels[runStart]) {
                    size_t remaining = i - runStart;
                    while (remaining > 0) {
                        const uint32_t run = static_cast<uint32_t>(
                            std::min<size_t>(remaining, UINT32_MAX));
                        appendU32(out, run);
                        appendU32(out, voxels[runStart]);
                        remaining -= run;
                    }
                    runStart = i;
                }
            }
            return out;
        }

        Header decodeHeader(const std::vector<char> &bytes, size_t &cursor,
                            const std::string &path) {
            if (bytes.size() < kHeaderBytes) {
                throw std::runtime_error("FileChunkStore: truncated chunk file: " + path);
            }
            Header h;
            h.magic = readU32(bytes, cursor, path);
            if (h.magic != kMagic) {
                throw std::runtime_error("FileChunkStore: bad magic in chunk file: " + path);
            }
            h.flags = readU32(bytes, cursor, path);
            h.voxelDims.x = static_cast<int32_t>(readU32(bytes, cursor, path));
            h.voxelDims.y = static_cast<int32_t>(readU32(bytes, cursor, path));
            h.voxelDims.z = static_cast<int32_t>(readU32(bytes, cursor, path));
            return h;
        }

        std::vector<uint32_t> decodePayload(const std::vector<char> &bytes, size_t cursor,
                                            size_t expectedVoxels, const std::string &path) {
            std::vector<uint32_t> voxels;
            voxels.reserve(expectedVoxels);
            while (cursor < bytes.size()) {
                const uint32_t run = readU32(bytes, cursor, path);
                const uint32_t value = readU32(bytes, cursor, path);
                if (run == 0 || voxels.size() + run > expectedVoxels) {
                    throw std::runtime_error("FileChunkStore: corrupt RLE in chunk file: " + path);
                }
                voxels.insert(voxels.end(), run, value);
            }
            if (voxels.size() != expectedVoxels) {
                throw std::runtime_error("FileChunkStore: corrupt RLE in chunk file: " + path);
            }
            return voxels;
        }
    }

    FileChunkStore::FileChunkStore(std::filesystem::path directory, BrickPool &pool,
                                   IVec3 chunkBrickDims, float voxelSize, YRange yRange)
        : directory_(std::move(directory))
        , pool_(pool)
        , chunkBrickDims_(chunkBrickDims)
        , voxelSize_(voxelSize)
        , yRange_(yRange) {
        std::filesystem::create_directories(directory_);
    }

    std::filesystem::path FileChunkStore::pathFor_(IVec3 coord) const {
        return directory_
               / ("chunk_" + std::to_string(coord.x) + "_" + std::to_string(coord.y) + "_"
                  + std::to_string(coord.z) + ".rgc");
    }

    bool FileChunkStore::hasChunkFile(IVec3 coord) const {
        const std::lock_guard lock(mtx_);
        return std::filesystem::exists(pathFor_(coord));
    }

    ChunkResult FileChunkStore::chunkAt(IVec3 coord) {
        const std::filesystem::path path = pathFor_(coord);
        std::vector<char> bytes;
        {
            const std::lock_guard lock(mtx_);
            std::ifstream in(path, std::ios::binary | std::ios::ate);
            if (!in.is_open()) {
                return ChunkResult{ .status = ChunkStatus::Missing, .chunk = nullptr };
            }
            const std::streamsize size = in.tellg();
            in.seekg(0);
            bytes.resize(static_cast<size_t>(size));
            if (!in.read(bytes.data(), size)) {
                throw std::runtime_error("FileChunkStore: failed to read chunk file: "
                                         + path.string());
            }
        }

        size_t cursor = 0;
        const Header h = decodeHeader(bytes, cursor, path.string());
        const IVec3 expectedDims = chunkBrickDims_ * 8;
        if (h.voxelDims.x != expectedDims.x || h.voxelDims.y != expectedDims.y
            || h.voxelDims.z != expectedDims.z) {
            throw std::runtime_error("FileChunkStore: chunk dims mismatch in file: "
                                     + path.string());
        }
        if ((h.flags & kFlagEmpty) != 0u) {
            return ChunkResult{ .status = ChunkStatus::Empty, .chunk = nullptr };
        }

        const std::vector<uint32_t> voxels =
            decodePayload(bytes, cursor, voxelCount(expectedDims), path.string());
        auto chunk = std::make_unique<Voxel3D>(pool_, expectedDims, voxelSize_);
        chunk->fillFromPackedRGBA8(voxels.data(), expectedDims);
        return ChunkResult{ .status = ChunkStatus::Ready, .chunk = std::move(chunk) };
    }

    void FileChunkStore::putChunk(IVec3 coord, const Voxel3D &chunk) {
        const IVec3 expectedDims = chunkBrickDims_ * 8;
        const IVec3 dims = chunk.dimensions();
        if (dims.x != expectedDims.x || dims.y != expectedDims.y || dims.z != expectedDims.z) {
            throw std::invalid_argument("FileChunkStore: putChunk dims mismatch");
        }

        const std::vector<char> bytes = encode(snapshotVoxels(chunk), dims);
        const std::filesystem::path path = pathFor_(coord);
        const std::filesystem::path tmpPath = path.string() + ".tmp";

        const std::lock_guard lock(mtx_);
        {
            std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
            if (!out.is_open() || !out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
                throw std::runtime_error("FileChunkStore: failed to write chunk file: "
                                         + tmpPath.string());
            }
        }
        std::filesystem::remove(path);
        std::filesystem::rename(tmpPath, path);
    }
}
