#include "RAGE_mesh.hpp"
#include "nlohmann/json.hpp"

struct GLBHeader
{
	uint32_t magic;
	uint32_t version;
	uint32_t length;
};

struct GLBChunkHeader
{
	uint32_t length;
	uint32_t type;
};

int RAGE_mesh::LoadGLB(const char *path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		std::cerr << "Failed to open file: " << path << std::endl;
		return -1;
	}

	GLBHeader header;
	file.read(reinterpret_cast<char *>(&header), sizeof(header));

	// Check the magic number
	if (header.magic != 0x46546C67)
	{
		std::cerr << "Invalid GLB file: " << path << std::endl;
		return -1;
	}

	GLBChunkHeader chunkHeader;
	file.read(reinterpret_cast<char *>(&chunkHeader), sizeof(chunkHeader));

	if (chunkHeader.type != 0x4E4F534A)
	{
		std::cerr << "Invalid GLB chunk type: " << chunkHeader.type << std::endl;
		return -1;
	}

	// Read the JSON chunk
	std::vector<char> json(chunkHeader.length);
	file.read(json.data(), json.size());

	// Parse the JSON
	nlohmann::json scene = nlohmann::json::parse(json.begin(), json.end());

	// Extract the vertices from the scene...
}