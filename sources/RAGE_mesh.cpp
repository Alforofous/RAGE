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
		return (-1);
	}

	GLBHeader header;
	file.read(reinterpret_cast<char *>(&header), sizeof(header));
	if (header.magic != 0x46546C67)
	{
		std::cerr << "Invalid GLB file: " << path << std::endl;
		return (-1);
	}

	GLBChunkHeader chunkHeader;
	file.read(reinterpret_cast<char *>(&chunkHeader), sizeof(chunkHeader));
	if (chunkHeader.type != 0x4E4F534A)
	{
		std::cerr << "Invalid GLB chunk type: " << chunkHeader.type << std::endl;
		return (-1);
	}

	std::vector<char> json(chunkHeader.length);
	file.read(json.data(), json.size());

	nlohmann::json scene = nlohmann::json::parse(json.begin(), json.end());

	std::vector<char> buffer(chunkHeader.length);
	file.read(buffer.data(), buffer.size());

	GLuint vertices_count;
	if (scene["buffers"][0].is_null() || scene["buffers"][0]["byteLength"].is_null())
	{
		std::cerr << "Invalid or missing byteLength in the first buffer" << std::endl;
		return -1;
	}
	else
	{
		this->vertices_size = scene["buffers"][0]["byteLength"];
		int accessorIndex = scene["meshes"][0]["primitives"][0]["attributes"]["POSITION"];

		int bufferViewIndex = scene["accessors"][accessorIndex]["bufferView"];

		int byteOffset = scene["bufferViews"][bufferViewIndex]["byteOffset"];
		int byteLength = scene["bufferViews"][bufferViewIndex]["byteLength"];

		vertices_count = byteLength / sizeof(GLfloat);
		std::vector<GLfloat> vertices(byteLength / sizeof(GLfloat));
		std::memcpy(vertices.data(), buffer.data() + byteOffset, byteLength);
		this->vertices = vertices.data();
	}

	if (scene["buffers"][1].is_null() || scene["buffers"][1]["byteLength"].is_null())
	{
		std::vector<GLuint> indices;
		for (GLuint i = 0; i < vertices_count; i++)
		{
			indices.push_back(i);
		}; 
		std::cerr << "Invalid or missing byteLength in the second buffer" << std::endl;
		this->indices = std::move(indices.data());
		this->indices_size = indices.size();
	}
	else
	{


	}
	printf("vertices_size: %ld\n", this->vertices_size);
	printf("indices_size: %ld\n", this->indices_size);
	return (1);
}