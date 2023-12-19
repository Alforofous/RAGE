#include "RAGE_mesh.hpp"
#include <fstream>
#include <iostream>

static void check_glb_header(const char *path, std::ifstream &file)
{
	GLBHeader header;
	file.read(reinterpret_cast<char *>(&header), sizeof(header));
	if (header.magic != 0x46546C67)
	{
		throw std::runtime_error("Invalid GLB file: " + std::string(path));
	}
}

static void check_glb_chunk_header(const char *path, std::ifstream &file, std::vector<char> &file_buffer, std::vector<char> &binary_file_buffer)
{
	GLBChunkHeader chunkHeader;
	file.read(reinterpret_cast<char *>(&chunkHeader), sizeof(chunkHeader));
	if (chunkHeader.type != 0x4E4F534A)
	{
		throw std::runtime_error("Invalid GLB chunk type: " + std::to_string(chunkHeader.type));
	}
	file_buffer.resize(chunkHeader.length);
	file.read(file_buffer.data(), file_buffer.size());

	GLBChunkHeader binaryChunkHeader;
	file.read(reinterpret_cast<char *>(&binaryChunkHeader), sizeof(binaryChunkHeader));
	if (binaryChunkHeader.type != 0x004E4942)
	{
		throw std::runtime_error("Invalid GLB binary chunk type: " + std::to_string(binaryChunkHeader.type));
	}
	binary_file_buffer.resize(binaryChunkHeader.length);
	file.read(binary_file_buffer.data(), binary_file_buffer.size());
}

void RAGE_mesh::load_model_vertex_colors(nlohmann::json &json_scene, std::vector<char> &binary_buffer)
{
	if (!json_scene["meshes"][0]["primitives"][0]["attributes"].contains("COLOR_0"))
		return;
	int colorAccessorIndex = json_scene["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"];
	if (json_scene["accessors"].is_null() || json_scene["accessors"][colorAccessorIndex].is_null())
	{
		throw std::runtime_error("Invalid or missing accessor for COLOR_0 attribute");
	}
	int bufferViewIndex = json_scene["accessors"][colorAccessorIndex]["bufferView"];
	if (json_scene["bufferViews"].is_null() || json_scene["bufferViews"][bufferViewIndex].is_null())
	{
		throw std::runtime_error("Invalid or missing buffer view for COLOR_0 attribute");
	}

	int byteOffset = json_scene["bufferViews"][bufferViewIndex]["byteOffset"];
	int byteLength = json_scene["bufferViews"][bufferViewIndex]["byteLength"];
	int componentType = json_scene["accessors"][colorAccessorIndex]["componentType"];
	std::string type = json_scene["accessors"][colorAccessorIndex]["type"];
	int color_channel_count = 4;
	if (type == "VEC3")
		color_channel_count = 3;

	this->vertex_colors = new GLfloat[this->vertices_count * color_channel_count];
	if (componentType == 5126) // FLOAT
		std::memcpy(this->vertex_colors, binary_buffer.data() + byteOffset, byteLength);
	else if (componentType == 5123) // UNSIGNED_SHORT
	{
		unsigned short *tempColors = new unsigned short[byteLength / sizeof(unsigned short)];
		std::memcpy(tempColors, binary_buffer.data() + byteOffset, byteLength);
		this->vertex_colors = new GLfloat[this->vertices_count * color_channel_count];
		for (int i = 0; i < this->vertices_count; i++)
		{
			this->vertex_colors[i * color_channel_count + 0] = tempColors[i * color_channel_count + 0] / 65535.0f;
			this->vertex_colors[i * color_channel_count + 1] = tempColors[i * color_channel_count + 1] / 65535.0f;
			this->vertex_colors[i * color_channel_count + 2] = tempColors[i * color_channel_count + 2] / 65535.0f;
			if (color_channel_count == 4)
				this->vertex_colors[i * color_channel_count + 3] = tempColors[i * color_channel_count + 3] / 65535.0f;
		}
		delete[] tempColors;
	}
	else
	{
		throw std::runtime_error("Unsupported componentType for COLOR_0 attribute");
	}

	for (int i = 0; i < this->vertices_count; i++)
	{
		this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 3] = this->vertex_colors[i * color_channel_count + 0];
		this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 4] = this->vertex_colors[i * color_channel_count + 1];
		this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 5] = this->vertex_colors[i * color_channel_count + 2];
		if (color_channel_count == 4)
			this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 6] = this->vertex_colors[i * color_channel_count + 3];
	}
}

void RAGE_mesh::load_model_vertex_positions(nlohmann::json &json_scene, std::vector<char> &binary_buffer)
{
	if (!json_scene["meshes"][0]["primitives"][0]["attributes"].contains("POSITION"))
	{
		throw std::runtime_error("No POSITION attribute found");
	}
	int positionAccessorIndex = json_scene["meshes"][0]["primitives"][0]["attributes"]["POSITION"];
	if (json_scene["accessors"].is_null() || json_scene["accessors"][positionAccessorIndex].is_null())
	{
		throw std::runtime_error("Invalid or missing accessor for POSITION attribute");
	}
	int bufferViewIndex = json_scene["accessors"][positionAccessorIndex]["bufferView"];
	if (json_scene["bufferViews"].is_null() || json_scene["bufferViews"][bufferViewIndex].is_null())
	{
		throw std::runtime_error("Invalid or missing buffer view for POSITION attribute");
	}

	int byteOffset = json_scene["bufferViews"][bufferViewIndex]["byteOffset"];
	int byteLength = json_scene["bufferViews"][bufferViewIndex]["byteLength"];

	std::vector<GLfloat> vertices(byteLength / sizeof(GLfloat));
	std::memcpy(vertices.data(), binary_buffer.data() + byteOffset, byteLength);

	std::vector<GLfloat> colors(byteLength / sizeof(GLfloat));
	std::fill(colors.begin(), colors.end(), 0.5f);
	this->vertices_count = byteLength / (3 * sizeof(GLfloat));
	this->vertices_size = this->vertices_count * VERTEX_ARRAY_ELEMENT_COUNT * sizeof(GLfloat);

	this->vertices = new GLfloat[this->vertices_count * VERTEX_ARRAY_ELEMENT_COUNT];
	for (int i = 0; i < this->vertices_count; i++)
	{
		this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 0] = vertices[i * 3 + 0];
		this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 1] = vertices[i * 3 + 1];
		this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 2] = vertices[i * 3 + 2];

		this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 3] = colors[i * 3 + 0];
		this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 4] = colors[i * 3 + 1];
		this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 5] = colors[i * 3 + 2];
		this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 6] = 1.0f;
	}
}

void RAGE_mesh::load_model_indices(nlohmann::json &json_scene, std::vector<char> &binary_buffer)
{
	if (!json_scene["meshes"][0]["primitives"][0].contains("indices"))
	{
		throw std::runtime_error("No INDICES attribute found");
	}
	int indicesAccessorIndex = json_scene["meshes"][0]["primitives"][0]["indices"];
	if (json_scene["accessors"].is_null() || json_scene["accessors"][indicesAccessorIndex].is_null())
	{
		throw std::runtime_error("Invalid or missing accessor for INDICES attribute");
	}
	int bufferViewIndex = json_scene["accessors"][indicesAccessorIndex]["bufferView"];
	if (json_scene["bufferViews"].is_null() || json_scene["bufferViews"][bufferViewIndex].is_null())
	{
		throw std::runtime_error("Invalid or missing buffer view for INDICES attribute");
	}

	int byteOffset = json_scene["bufferViews"][bufferViewIndex]["byteOffset"];
	int byteLength = json_scene["bufferViews"][bufferViewIndex]["byteLength"];
	int componentType = json_scene["accessors"][indicesAccessorIndex]["componentType"];
	if (componentType == 5123)
	{
		GLushort *tempIndices = new GLushort[byteLength / sizeof(GLushort)];
		std::memcpy(tempIndices, binary_buffer.data() + byteOffset, byteLength);
		this->indices = new GLuint[byteLength / sizeof(GLushort)];
		for (int i = 0; i < byteLength / sizeof(GLushort); i++)
			this->indices[i] = static_cast<GLuint>(tempIndices[i]);
		delete[] tempIndices;
		this->indices_count = byteLength / sizeof(GLushort);
	}
	else if (componentType == 5125)
	{
		this->indices = new GLuint[byteLength / sizeof(GLuint)];
		std::memcpy(this->indices, binary_buffer.data() + byteOffset, byteLength);
		this->indices_count = byteLength / sizeof(GLuint);
	}
	else
	{
		throw std::runtime_error("Unsupported componentType for INDICES attribute");
	}
	this->indices_size = this->indices_count * sizeof(GLuint);
}

bool RAGE_mesh::LoadGLB(const char *path)
{
	std::ifstream file;
	std::vector<char> json_header_buffer;
	std::vector<char> binary_buffer;

	try
	{
		file.open(path, std::ios::binary);
		if (!file)
			throw std::runtime_error("Failed to open file: " + std::string(path));
		check_glb_header(path, file);
		check_glb_chunk_header(path, file, json_header_buffer, binary_buffer);

		nlohmann::json json_scene = nlohmann::json::parse(json_header_buffer.begin(), json_header_buffer.end());
		if (json_scene["meshes"].is_null() || json_scene["meshes"][0]["primitives"].is_null() || json_scene["meshes"][0]["primitives"][0]["attributes"].is_null())
			throw std::runtime_error("Invalid or missing attributes in the first primitive of the first mesh");
		load_model_vertex_positions(json_scene, binary_buffer);
		load_model_vertex_colors(json_scene, binary_buffer);
		load_model_indices(json_scene, binary_buffer);

		for (int i = 0; i < this->vertices_count; i += 1)
			printf("v[%d]: %.2f %.2f %.2f %.2f %.2f %.2f\n", i, this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 0], this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 1], this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 2], this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 3], this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 4], this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 5]);
		for (int i = 0; i < this->indices_count; i += 1)
			printf("i[%d]: %u\n", i, this->indices[i]);
		printf("vertices_count: %u\n", this->vertices_count);
		printf("indices_count: %u\n", this->indices_count);
		return (true);
	}
	catch (const std::exception &e)
	{
		std::cerr << "RAGE_mesh error: " << e.what() << std::endl;
		return (false);
	}
}