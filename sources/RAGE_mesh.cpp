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

static int read_file(const char *path, std::ifstream &file)
{
	file.open(path, std::ios::binary);
	if (!file)
	{
		std::cerr << "Failed to open file: " << path << std::endl;
		return (-1);
	}
	return (1);
}

static int check_glb_header(const char *path, std::ifstream &file)
{
	GLBHeader header;
	file.read(reinterpret_cast<char *>(&header), sizeof(header));
	if (header.magic != 0x46546C67)
	{
		std::cerr << "Invalid GLB file: " << path << std::endl;
		return (-1);
	}

	return (1);
}

static int check_glb_chunk_header(const char *path, std::ifstream &file, std::vector<char> &file_buffer, std::vector<char> &binary_file_buffer)
{
	GLBChunkHeader chunkHeader;
	file.read(reinterpret_cast<char *>(&chunkHeader), sizeof(chunkHeader));
	if (chunkHeader.type != 0x4E4F534A)
	{
		std::cerr << "Invalid GLB chunk type: " << chunkHeader.type << std::endl;
		return (-1);
	}
	file_buffer.resize(chunkHeader.length);
	file.read(file_buffer.data(), file_buffer.size());

	GLBChunkHeader binaryChunkHeader;
	file.read(reinterpret_cast<char *>(&binaryChunkHeader), sizeof(binaryChunkHeader));
	if (binaryChunkHeader.type != 0x004E4942)
	{
		std::cerr << "Invalid GLB binary chunk type: " << binaryChunkHeader.type << std::endl;
		return (-1);
	}
	binary_file_buffer.resize(binaryChunkHeader.length);
	file.read(binary_file_buffer.data(), binary_file_buffer.size());
	return (1);
}

int RAGE_mesh::LoadGLB(const char *path)
{
	std::ifstream		file;
	std::vector<char>	json_file_buffer;
	std::vector<char>	binary_file_buffer;

	if (read_file(path, file) == -1)
		return (-1);
	if (check_glb_header(path, file) == -1)
		return (-1);
	if (check_glb_chunk_header(path, file, json_file_buffer, binary_file_buffer) == -1)
		return (-1);

	nlohmann::json json_scene = nlohmann::json::parse(json_file_buffer.begin(), json_file_buffer.end());

	GLuint vertices_count;
	if (json_scene["meshes"].is_null() || json_scene["meshes"][0]["primitives"].is_null() || json_scene["meshes"][0]["primitives"][0]["attributes"].is_null() || json_scene["meshes"][0]["primitives"][0]["attributes"]["POSITION"].is_null())
	{
		std::cerr << "Invalid or missing POSITION attribute in the first primitive of the first mesh" << std::endl;
		return -1;
	}
	else
	{
		int positionAccessorIndex = json_scene["meshes"][0]["primitives"][0]["attributes"]["POSITION"];
		if (json_scene["accessors"].is_null() || json_scene["accessors"][positionAccessorIndex].is_null())
		{
			std::cerr << "Invalid or missing accessor for POSITION attribute" << std::endl;
			return -1;
		}
		else
		{
			int bufferViewIndex = json_scene["accessors"][positionAccessorIndex]["bufferView"];
			if (json_scene["bufferViews"].is_null() || json_scene["bufferViews"][bufferViewIndex].is_null())
			{
				std::cerr << "Invalid or missing buffer view for POSITION attribute" << std::endl;
				return -1;
			}
			else
			{
				int byteOffset = json_scene["bufferViews"][bufferViewIndex]["byteOffset"];
				int byteLength = json_scene["bufferViews"][bufferViewIndex]["byteLength"];

				printf("byteOffset: %d byteLength: %d\n", byteOffset, byteLength);

				std::vector<GLfloat> vertices(byteLength / sizeof(GLfloat));
				std::memcpy(vertices.data(), binary_file_buffer.data() + byteOffset, byteLength);

				std::vector<GLfloat> colors(byteLength / sizeof(GLfloat));
				std::fill(colors.begin(), colors.end(), 0.8f);
				vertices_count = byteLength / (3 * sizeof(GLfloat));
				this->vertices_size = vertices_count * 6 * sizeof(GLfloat);

				printf("vertices_count: %d\n", vertices_count);
				printf("vertices_countf: %f\n", (float)byteLength / (3.0f * sizeof(GLfloat)));

				this->vertices = new GLfloat[vertices_count * 6];
				for (int i = 0; i < vertices_count; i++)
				{
					this->vertices[i * 6 + 0] = vertices[i * 3 + 0];
					this->vertices[i * 6 + 1] = vertices[i * 3 + 1];
					this->vertices[i * 6 + 2] = vertices[i * 3 + 2];
					this->vertices[i * 6 + 3] = colors[i * 3 + 0];
					this->vertices[i * 6 + 4] = colors[i * 3 + 1];
					this->vertices[i * 6 + 5] = colors[i * 3 + 2];
				}

				std::vector<GLfloat> unique_vertices;
				for (int i = 0; i < vertices.size(); i += 3)
				{
					GLfloat x = vertices[i];
					GLfloat y = vertices[i + 1];
					GLfloat z = vertices[i + 2];

					bool is_unique = true;
					for (int j = 0; j < unique_vertices.size(); j += 3)
					{
						if (unique_vertices[j] == x && unique_vertices[j + 1] == y && unique_vertices[j + 2] == z)
						{
							is_unique = false;
							break;
						}
					}
					if (is_unique)
					{
						unique_vertices.push_back(x);
						unique_vertices.push_back(y);
						unique_vertices.push_back(z);
					}
				}
				printf("unique_vertices.size(): %zu\n", unique_vertices.size());

			}
		}
	}

	if (json_scene["meshes"][0]["primitives"][0].contains("indices"))
	{
		int indicesAccessorIndex = json_scene["meshes"][0]["primitives"][0]["indices"];
		if (json_scene["accessors"].is_null() || json_scene["accessors"][indicesAccessorIndex].is_null())
		{
			std::cerr << "Invalid or missing accessor for INDICES attribute" << std::endl;
			return -1;
		}
		else
		{
			int bufferViewIndex = json_scene["accessors"][indicesAccessorIndex]["bufferView"];
			if (json_scene["bufferViews"].is_null() || json_scene["bufferViews"][bufferViewIndex].is_null())
			{
				std::cerr << "Invalid or missing buffer view for INDICES attribute" << std::endl;
				return -1;
			}
			else
			{
				int byteOffset = json_scene["bufferViews"][bufferViewIndex]["byteOffset"];
				int byteLength = json_scene["bufferViews"][bufferViewIndex]["byteLength"];
				int componentType = json_scene["accessors"][indicesAccessorIndex]["componentType"];

				GLuint indices_count;
				if (componentType == 5123)
				{
					printf("GLushort\n");
					GLushort *tempIndices = new GLushort[byteLength / sizeof(GLushort)];
					std::memcpy(tempIndices, binary_file_buffer.data() + byteOffset, byteLength);
					this->indices = new GLuint[byteLength / sizeof(GLushort)];
					for (int i = 0; i < byteLength / sizeof(GLushort); i++)
						this->indices[i] = static_cast<GLuint>(tempIndices[i]);
					delete[] tempIndices;
					indices_count = byteLength / sizeof(GLushort);
				}
				else if (componentType == 5125)
				{
					printf("GLuint\n");
					this->indices = new GLuint[byteLength / sizeof(GLuint)];
					std::memcpy(this->indices, binary_file_buffer.data() + byteOffset, byteLength);
					indices_count = byteLength / sizeof(GLuint);
				}
				else
				{
					std::cerr << "Unsupported componentType for INDICES attribute" << std::endl;
					return -1;
				}
				this->indices_size = indices_count * sizeof(GLuint);
			}
		}
	}
	else
	{
		std::cerr << "No INDICES attribute found" << std::endl;
		return -1;
	}

	for (int i = 0; i < vertices_count; i += 1)
	{
		printf("v[%d]: %.2f %.2f %.2f %.2f %.2f %.2f index: %u\n", i, this->vertices[i * 6 + 0], this->vertices[i * 6 + 1], this->vertices[i * 6 + 2], this->vertices[i * 6 + 3], this->vertices[i * 6 + 4], this->vertices[i * 6 + 5], this->indices[i]);
	}
	return (1);
}