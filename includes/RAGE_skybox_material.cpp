#include "RAGE_skybox_material.hpp"
#include "RAGE.hpp"

RAGE_skybox_material::RAGE_skybox_material()
{
	RAGE *rage = get_rage();

	shader = rage->skybox_shader;
	glGenTextures(1, &this->cube_sampler_id);
	glBindTexture(GL_TEXTURE_CUBE_MAP, this->cube_sampler_id);

	this->load_texture((rage->executable_path + "/assets/textures/skybox/right.jpg").c_str(), 0);
	this->load_texture((rage->executable_path + "/assets/textures/skybox/left.jpg").c_str(), 1);
	this->load_texture((rage->executable_path + "/assets/textures/skybox/top.jpg").c_str(), 2);
	this->load_texture((rage->executable_path + "/assets/textures/skybox/bottom.jpg").c_str(), 3);
	this->load_texture((rage->executable_path + "/assets/textures/skybox/front.jpg").c_str(), 4);
	this->load_texture((rage->executable_path + "/assets/textures/skybox/back.jpg").c_str(), 5);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glUniform1i(this->shader->uniforms["u_skybox"].location, 0);
}

bool RAGE_skybox_material::load_texture(const char *path, int index)
{
	if (index < 0 || index > 5)
		return (false);
	if (this->textures[index].load(path) == false)
		return (false);
	
	glBindTexture(GL_TEXTURE_CUBE_MAP, this->cube_sampler_id);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + index, 0, GL_RGB, this->textures[index].get_width(), this->textures[index].get_height(), 0, GL_RGB, GL_UNSIGNED_BYTE, this->textures[index].get_data());
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	return (true);
}

RAGE_skybox_material::~RAGE_skybox_material()
{
	glDeleteTextures(1, &this->cube_sampler_id);
}