#include "RAGE_material_skybox.hpp"
#include "RAGE.hpp"

RAGE_material_skybox::RAGE_material_skybox()
{
	RAGE *rage = get_rage();

	this->name = "Skybox material";
	this->shader = rage->skybox_shader;
	glGenTextures(1, &this->cube_sampler_id);
	glBindTexture(GL_TEXTURE_CUBE_MAP, this->cube_sampler_id);

	this->load_texture((rage->executable_path + "/assets/textures/skybox/right.jpg").c_str(), 0);
	this->load_texture((rage->executable_path + "/assets/textures/skybox/right.jpg").c_str(), 1);
	this->load_texture((rage->executable_path + "/assets/textures/skybox/right.jpg").c_str(), 2);
	this->load_texture((rage->executable_path + "/assets/textures/skybox/right.jpg").c_str(), 3);
	this->load_texture((rage->executable_path + "/assets/textures/skybox/right.jpg").c_str(), 4);
	this->load_texture((rage->executable_path + "/assets/textures/skybox/right.jpg").c_str(), 5);
	/*
	this->load_texture((rage->executable_path + "/assets/textures/skybox/left.jpg").c_str(), 1);
	this->load_texture((rage->executable_path + "/assets/textures/skybox/top.jpg").c_str(), 2);
	this->load_texture((rage->executable_path + "/assets/textures/skybox/bottom.jpg").c_str(), 3);
	this->load_texture((rage->executable_path + "/assets/textures/skybox/front.jpg").c_str(), 4);
	this->load_texture((rage->executable_path + "/assets/textures/skybox/back.jpg").c_str(), 5);
	*/

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glUseProgram(this->shader->program);
	glUniform1i(this->shader->uniforms["u_skybox"].location, 0);
}

bool RAGE_material_skybox::load_texture(const char *path, int index)
{
	if (index < 0 || index > 5)
		return (false);
	if (this->textures[index].load_texture_data(path) == false)
		this->textures[index].create_empty_texture(this->textures[0].get_width(), this->textures[0].get_height(), this->textures[0].get_channel_count());

	printf("First pixel of texture %s: %d %d %d\n", path, this->textures[index].get_data()[0], this->textures[index].get_data()[1], this->textures[index].get_data()[2]);
	printf("Dimensions of texture %s: %d %d\n", path, this->textures[index].get_width(), this->textures[index].get_height());

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, this->cube_sampler_id);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + index, 0, GL_RGB, this->textures[index].get_width(), this->textures[index].get_height(), 0, GL_RGB, GL_UNSIGNED_BYTE, this->textures[index].get_data());
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	return (true);
}

RAGE_material_skybox::~RAGE_material_skybox()
{
	glDeleteTextures(1, &this->cube_sampler_id);
}