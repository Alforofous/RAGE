// Generates the demo scene's voxel assets as MagicaVoxel .vox files under the directory
// given as argv[1]. Outputs:
//   - <dir>/sphere.vox  : 24^3 gradient-colored sphere of radius ~10 voxels.
//   - <dir>/floor.vox   : 96 x 1 x 96 grey floor slab.
//
// Built and executed as part of the CMake build so the demo app finds these files at run
// time. Replaceable: overwrite either output with any real MagicaVoxel export (or change
// this generator) to swap the demo content.

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {
    constexpr uint32_t kMagicVox = 0x20584F56u;
    constexpr uint32_t kChunkMain = 0x4E49414Du;
    constexpr uint32_t kChunkSize = 0x455A4953u;
    constexpr uint32_t kChunkXyzi = 0x495A5958u;
    constexpr uint32_t kChunkRgba = 0x41424752u;
    constexpr uint32_t kVersion = 150u;

    void writeU32(std::vector<uint8_t> &buf, uint32_t v) {
        const std::array<uint8_t, 4> bytes{
            static_cast<uint8_t>(v & 0xFFu),
            static_cast<uint8_t>((v >> 8u) & 0xFFu),
            static_cast<uint8_t>((v >> 16u) & 0xFFu),
            static_cast<uint8_t>((v >> 24u) & 0xFFu),
        };
        buf.insert(buf.end(), bytes.begin(), bytes.end());
    }

    void writeI32(std::vector<uint8_t> &buf, int32_t v) {
        writeU32(buf, static_cast<uint32_t>(v));
    }

    struct Voxel {
        uint8_t x;
        uint8_t y;
        uint8_t z;
        uint8_t colorIdx;
    };

    uint32_t packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8u) | (static_cast<uint32_t>(b) << 16u)
               | (static_cast<uint32_t>(a) << 24u);
    }

    void writeVoxFile(const std::filesystem::path &path, int32_t dimX, int32_t dimY, int32_t dimZ,
                      const std::vector<Voxel> &voxels, const std::array<uint32_t, 256> &palette) {
        std::vector<uint8_t> children;

        writeU32(children, kChunkSize);
        writeU32(children, 12);
        writeU32(children, 0);
        writeI32(children, dimX);
        writeI32(children, dimY);
        writeI32(children, dimZ);

        writeU32(children, kChunkXyzi);
        writeU32(children, 4u + (static_cast<uint32_t>(voxels.size()) * 4u));
        writeU32(children, 0);
        writeU32(children, static_cast<uint32_t>(voxels.size()));
        for (const Voxel &v : voxels) {
            children.push_back(v.x);
            children.push_back(v.y);
            children.push_back(v.z);
            children.push_back(v.colorIdx);
        }

        writeU32(children, kChunkRgba);
        writeU32(children, 1024);
        writeU32(children, 0);
        children.insert(children.end(), reinterpret_cast<const uint8_t *>(palette.data()),
                        reinterpret_cast<const uint8_t *>(palette.data()) + 1024);

        std::vector<uint8_t> file;
        writeU32(file, kMagicVox);
        writeU32(file, kVersion);
        writeU32(file, kChunkMain);
        writeU32(file, 0);
        writeU32(file, static_cast<uint32_t>(children.size()));
        file.insert(file.end(), children.begin(), children.end());

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open()) {
            std::fprintf(stderr, "make_sample_vox: failed to open %s for writing\n", path.string().c_str());
            std::exit(1);
        }
        out.write(reinterpret_cast<const char *>(file.data()), static_cast<std::streamsize>(file.size()));
    }

    // Sphere: 256^3 grid, hollow shell of radius ~120 voxels (3 voxels thick) centred on the
    // grid centre. Mostly empty interior stresses intra-chunk mip-skipping. Coloured with an
    // (x, y, z) gradient binned into 6 levels per axis (6^3 = 216 palette entries) so the
    // gradient is preserved while staying inside MagicaVoxel's 256-color palette budget.
    void writeSphereVox(const std::filesystem::path &outDir) {
        constexpr int32_t kDim = 256;
        constexpr int kBins = 6;
        constexpr int kVoxelsPerBin = kDim / kBins;
        constexpr float kCentre = 127.5f;
        constexpr float kOuterRadius = 120.0f;
        constexpr float kInnerRadius = 117.0f;

        std::vector<Voxel> voxels;
        voxels.reserve(static_cast<size_t>(kDim) * kDim * kDim);
        for (int32_t z = 0; z < kDim; ++z) {
            for (int32_t y = 0; y < kDim; ++y) {
                for (int32_t x = 0; x < kDim; ++x) {
                    const float dx = static_cast<float>(x) - kCentre;
                    const float dy = static_cast<float>(y) - kCentre;
                    const float dz = static_cast<float>(z) - kCentre;
                    const float r = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
                    if (r >= kOuterRadius || r < kInnerRadius) {
                        continue;
                    }
                    const int bx = x / kVoxelsPerBin;
                    const int by = y / kVoxelsPerBin;
                    const int bz = z / kVoxelsPerBin;
                    const int paletteIndex = (bx * kBins * kBins) + (by * kBins) + bz;
                    voxels.push_back(Voxel{ .x = static_cast<uint8_t>(x),
                                            .y = static_cast<uint8_t>(y),
                                            .z = static_cast<uint8_t>(z),
                                            .colorIdx = static_cast<uint8_t>(paletteIndex + 1) });
                }
            }
        }

        std::array<uint32_t, 256> palette{};
        for (int bx = 0; bx < kBins; ++bx) {
            for (int by = 0; by < kBins; ++by) {
                for (int bz = 0; bz < kBins; ++bz) {
                    const float r = (static_cast<float>(bx) + 0.5f) / static_cast<float>(kBins);
                    const float g = (static_cast<float>(by) + 0.5f) / static_cast<float>(kBins);
                    const float b = (static_cast<float>(bz) + 0.5f) / static_cast<float>(kBins);
                    const int idx = (bx * kBins * kBins) + (by * kBins) + bz;
                    palette[idx] = packRGBA(static_cast<uint8_t>(r * 255.0f), static_cast<uint8_t>(g * 255.0f),
                                            static_cast<uint8_t>(b * 255.0f), 0xFF);
                }
            }
        }

        writeVoxFile(outDir / "sphere.vox", kDim, kDim, kDim, voxels, palette);
    }

    // Floor: 256 x 1 x 256 solid grey slab.
    void writeFloorVox(const std::filesystem::path &outDir) {
        constexpr int32_t kDimXZ = 256;
        constexpr int32_t kDimY = 1;

        std::vector<Voxel> voxels;
        voxels.reserve(static_cast<size_t>(kDimXZ) * kDimXZ);
        for (int32_t z = 0; z < kDimXZ; ++z) {
            for (int32_t x = 0; x < kDimXZ; ++x) {
                voxels.push_back(Voxel{ .x = static_cast<uint8_t>(x),
                                        .y = 0,
                                        .z = static_cast<uint8_t>(z),
                                        .colorIdx = 1 });
            }
        }

        std::array<uint32_t, 256> palette{};
        palette[0] = packRGBA(0xA6, 0xA6, 0xA6, 0xFF);

        writeVoxFile(outDir / "floor.vox", kDimXZ, kDimY, kDimXZ, voxels, palette);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: make_sample_vox <output-directory>\n");
        return 1;
    }

    const std::filesystem::path outDir = argv[1];
    writeSphereVox(outDir);
    writeFloorVox(outDir);
    return 0;
}
