#include "RAGE_mesh.hpp"
#include <fstream>
#include <iostream>

RAGE_mesh::RAGE_mesh()
{
	this->vertices = NULL;
	this->indices = NULL;
}

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
	this->vertex_color_channel_count = 4;
	if (type == "VEC3")
		this->vertex_color_channel_count = 3;

	this->vertex_colors.clear();
	if (componentType == 5126) // FLOAT
	{
		this->vertex_colors.resize(byteLength / sizeof(GLfloat));
		std::memcpy(this->vertex_colors.data(), binary_buffer.data() + byteOffset, byteLength);
		printf("MESH COLORS COUNT: %d\n", byteLength / sizeof(GLfloat));
	}
	else if (componentType == 5123) // UNSIGNED_SHORT
	{
		unsigned short *tempColors = new unsigned short[byteLength / sizeof(unsigned short)];
		std::memcpy(tempColors, binary_buffer.data() + byteOffset, byteLength);
		for (GLuint i = 0; i < this->vertices_count; i++)
		{
			int vertex_color_index = i * this->vertex_color_channel_count;
			this->vertex_colors.push_back(tempColors[vertex_color_index + 0] / 65535.0f);
			this->vertex_colors.push_back(tempColors[vertex_color_index + 1] / 65535.0f);
			this->vertex_colors.push_back(tempColors[vertex_color_index + 2] / 65535.0f);
			if (this->vertex_color_channel_count == 4)
				this->vertex_colors.push_back(tempColors[vertex_color_index + 3] / 65535.0f);
		}
		delete[] tempColors;
	}
	else
	{
		throw std::runtime_error("Unsupported componentType for COLOR_0 attribute");
	}
}

void RAGE_mesh::load_model_vertex_positions(nlohmann::json &json_scene, std::vector<char> &binary_buffer)
{
	if (!json_scene["meshes"][0]["primitives"][0]["attributes"].contains("POSITION"))
		throw std::runtime_error("No POSITION attribute found");
	int positionAccessorIndex = json_scene["meshes"][0]["primitives"][0]["attributes"]["POSITION"];
	if (json_scene["accessors"].is_null() || json_scene["accessors"][positionAccessorIndex].is_null())
		throw std::runtime_error("Invalid or missing accessor for POSITION attribute");
	int bufferViewIndex = json_scene["accessors"][positionAccessorIndex]["bufferView"];
	if (json_scene["bufferViews"].is_null() || json_scene["bufferViews"][bufferViewIndex].is_null())
		throw std::runtime_error("Invalid or missing buffer view for POSITION attribute");
	if (json_scene["accessors"][positionAccessorIndex].contains("count") == false)
		throw std::runtime_error("Invalid or missing count for POSITION attribute");
	int count = json_scene["accessors"][positionAccessorIndex]["count"];
	int byteOffset = 0;
	if (json_scene["accessors"][positionAccessorIndex].contains("byteOffset") == true)
		byteOffset = json_scene["accessors"][positionAccessorIndex]["byteOffset"];
	int byteStride = 3 * sizeof(GLfloat);
	if (json_scene["bufferViews"][bufferViewIndex].contains("byteStride"))
		byteStride = json_scene["bufferViews"][bufferViewIndex]["byteStride"];

	std::vector<GLfloat> vertices(count * 3);
	printf("MESH VERTICES COUNT: %d\n", count);
	std::memcpy(vertices.data(), binary_buffer.data() + byteOffset, byteLength);
	
	this->vertices_count = count;
	this->vertices_size = this->vertices_count * VERTEX_ARRAY_ELEMENT_COUNT * sizeof(GLfloat);

	this->vertex_positions.clear();
	for (GLuint i = 0; i < this->vertices_count; i++)
	{
		int vertex_position_index = i * VERTEX_POSITION_ELEMENT_COUNT;
		this->vertex_positions.push_back(vertices[vertex_position_index + 0]);
		this->vertex_positions.push_back(vertices[vertex_position_index + 1]);
		this->vertex_positions.push_back(vertices[vertex_position_index + 2]);
	}
	std::vector<GLfloat> colors(count);
	std::fill(colors.begin(), colors.end(), 0.5f);
	this->vertex_color_channel_count = 3;
	this->vertex_colors.clear();
	for (GLuint i = 0; i < this->vertices_count; i++)
	{
		int vertex_color_index = i * this->vertex_color_channel_count;
		this->vertex_colors.push_back(colors[vertex_color_index + 0]);
		this->vertex_colors.push_back(colors[vertex_color_index + 1]);
		this->vertex_colors.push_back(colors[vertex_color_index + 2]);
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
	if (componentType == 5123) // UNSIGNED_SHORT
	{
		GLushort *tempIndices = new GLushort[byteLength / sizeof(GLushort)];
		std::memcpy(tempIndices, binary_buffer.data() + byteOffset, byteLength);
		this->indices = new GLuint[byteLength / sizeof(GLushort)];
		for (int i = 0; i < byteLength / sizeof(GLushort); i++)
			this->indices[i] = static_cast<GLuint>(tempIndices[i]);
		delete[] tempIndices;
		this->indices_count = byteLength / sizeof(GLushort);
		this->indices_size = this->indices_count * sizeof(GLuint);
	}
	else if (componentType == 5125) // UNSIGNED_INT
	{
		this->indices = new GLuint[byteLength / sizeof(GLuint)];
		std::memcpy(this->indices, binary_buffer.data() + byteOffset, byteLength);
		this->indices_count = byteLength / sizeof(GLuint);
		this->indices_size = this->indices_count * sizeof(GLuint);
	}
	else
	{
		throw std::runtime_error("Unsupported componentType for INDICES attribute");
	}
}

void RAGE_mesh::set_vertex_positions(GLfloat *vertex_positions, unsigned int count)
{
	this->vertex_positions.clear();
	this->vertex_positions.assign(vertex_positions, vertex_positions + count);
	this->set_vertex_positions();
}

void RAGE_mesh::set_vertex_colors(GLfloat *vertex_colors, unsigned int count)
{
	this->vertex_colors.clear();
	this->vertex_colors.assign(vertex_colors, vertex_colors + count);
	this->set_vertex_colors();
}

void RAGE_mesh::set_indices(GLuint *indices, unsigned int count)
{
	this->indices_count = count;
	this->indices_size = this->indices_count * sizeof(GLuint);
	if (this->indices != NULL)
		delete[] this->indices;
	this->indices = new GLuint[this->indices_count];
	std::memcpy(this->indices, indices, this->indices_size);
	this->indices_size = this->indices_count * sizeof(GLuint);
}

void RAGE_mesh::set_vertex_positions()
{
	GLuint vertices_count = (GLuint)(this->vertex_positions.size() / VERTEX_POSITION_ELEMENT_COUNT);
	this->vertices = new GLfloat[vertices_count * VERTEX_ARRAY_ELEMENT_COUNT];

	std::fill_n(this->vertices, vertices_count * VERTEX_ARRAY_ELEMENT_COUNT, 0.75f);
	for (GLuint i = 0; i < vertices_count; i++)
	{
		int vertex_position_index = i * VERTEX_POSITION_ELEMENT_COUNT;
		int vertices_index = i * VERTEX_ARRAY_ELEMENT_COUNT;

		this->vertices[vertices_index + 0] = this->vertex_positions[vertex_position_index + 0];
		this->vertices[vertices_index + 1] = this->vertex_positions[vertex_position_index + 1];
		this->vertices[vertices_index + 2] = this->vertex_positions[vertex_position_index + 2];

			this->vertices[vertices_index + 6] = 1.0f;
	}
	this->vertices_size = vertices_count * VERTEX_ARRAY_ELEMENT_COUNT * sizeof(GLfloat);
}

void RAGE_mesh::set_vertex_colors()
{
	GLuint vertices_count = (GLuint)(this->vertex_colors.size() / this->vertex_color_channel_count);

	for (GLuint i = 0; i < vertices_count; i++)
	{
		int vertex_color_index = i * this->vertex_color_channel_count;
		int vertices_index = i * VERTEX_ARRAY_ELEMENT_COUNT;

		this->vertices[vertices_index + 3] = this->vertex_colors[vertex_color_index + 0];
		this->vertices[vertices_index + 4] = this->vertex_colors[vertex_color_index + 1];
		this->vertices[vertices_index + 5] = this->vertex_colors[vertex_color_index + 2];

		if (this->vertex_color_channel_count == 4)
			this->vertices[vertices_index + 6] = this->vertex_colors[vertex_color_index + 3];
		else
			this->vertices[vertices_index + 6] = 1.0f;
	}
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
		set_vertex_positions();
		set_vertex_colors();
		load_model_indices(json_scene, binary_buffer);

		/*
		for (GLuint i = 0; i < this->vertices_count; i += 1)
			printf("v[%d]: %.2f %.2f %.2f %.2f %.2f %.2f\n", i, this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 0], this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 1], this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 2], this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 3], this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 4], this->vertices[i * VERTEX_ARRAY_ELEMENT_COUNT + 5]);
		for (int i = 0; i < this->indices_count; i += 1)
			printf("i[%d]: %u\n", i, this->indices[i]);
		*/
		printf("vertices_count: %u\n", this->vertices_count);
		printf("indices_count: %u\n", this->indices_count);
		return (true);
	}
	catch (const std::exception &e)
	{
		std::cerr << "RAGE_mesh " << path << " error: " << e.what() << std::endl;
		return (false);
	}
}