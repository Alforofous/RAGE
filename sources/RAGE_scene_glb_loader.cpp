#include "RAGE_scene.hpp"
#include "nlohmann/json.hpp"
#include <fstream>
#include <iostream>

static bool check_glb_header(std::ifstream &file)
{
	GLBHeader header;

	file.read(reinterpret_cast<char *>(&header), sizeof(header));
	if (header.magic != 0x46546C67)
		return (false);
	return (true);
}

static bool check_glb_chunk_header(std::ifstream &file, std::vector<char> &header_buffer, uint32_t type)
{
	GLBChunkHeader chunkHeader;

	file.read(reinterpret_cast<char *>(&chunkHeader), sizeof(chunkHeader));
	if (chunkHeader.type != type)
		return (false);
	header_buffer.resize(chunkHeader.length);
	file.read(header_buffer.data(), header_buffer.size());
	return (true);
}

void RAGE_scene::read_scene_info_GLB(nlohmann::json &json)
{
	if (json["scene"].is_null() || json["scenes"].is_null())
		throw std::runtime_error("No default scene or scenes array.");
	int sceneIndex = json["scene"];
	if (sceneIndex < 0 || sceneIndex >= json["scenes"].size())
		throw std::runtime_error("Invalid scene index.");
	nlohmann::json scene = json["scenes"][sceneIndex];
	std::string scene_name = "GlB Scene";
	if (scene["name"].is_null())
		this->name = scene_name;
	else
		this->name = scene["name"];
}

bool RAGE_scene::load_from_GLB(const char *path)
{
	std::ifstream file;
	std::vector<char> json_header_buffer;
	std::vector<char> binary_buffer;

	try
	{
		file.open(path, std::ios::binary);
		if (!file)
			throw std::runtime_error("Failed to open file.");
		if (check_glb_header(file) == false)
			throw std::runtime_error("Invalid GLB file.");
		if (check_glb_chunk_header(file, json_header_buffer, 0x4E4F534A) == false)
			throw std::runtime_error("Invalid GLB json header.");
		if (check_glb_chunk_header(file, binary_buffer, 0x004E4942) == false)
			throw std::runtime_error("Invalid GLB binary buffer.");

		printf("json_header:\n %s\n", json_header_buffer.data());
		printf("binary_buffer:\n %s\n", binary_buffer.data());

		nlohmann::json json = nlohmann::json::parse(json_header_buffer.begin(), json_header_buffer.end());
		read_scene_info_GLB(json);
		return (true);
	}
	catch (const std::exception &e)
	{
		std::cerr << "RAGE_scene::load_from_GLB at \"" << path << "\" has error: " << e.what() << std::endl;
		return (false);
	}
}