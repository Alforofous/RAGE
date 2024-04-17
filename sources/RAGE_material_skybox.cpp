#include "RAGE_material_skybox.hpp"
#include "RAGE.hpp"

RAGE_material_skybox::RAGE_material_skybox()
{
	RAGE *rage = get_rage();

	this->name = "Skybox material";
	this->shader = rage->skybox_shader;

	this->textures.resize(6);
	
	this->textures[0].load_gl_texture((rage->executable_path + "/assets/textures/skybox/right.jpg").c_str());
	this->textures[1].load_gl_texture((rage->executable_path + "/assets/textures/skybox/left.jpg").c_str());
	this->textures[2].load_gl_texture((rage->executable_path + "/assets/textures/skybox/top.jpg").c_str());
	this->textures[3].load_gl_texture((rage->executable_path + "/assets/textures/skybox/bottom.jpg").c_str());
	this->textures[4].load_gl_texture((rage->executable_path + "/assets/textures/skybox/front.jpg").c_str());
	this->textures[5].load_gl_texture((rage->executable_path + "/assets/textures/skybox/back.jpg").c_str());

	glGenTextures(1, &this->cube_sampler_id);
	glBindTexture(GL_TEXTURE_CUBE_MAP, this->cube_sampler_id);
	for (int i = 0; i < 6; i++)
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textures[i].get_id());
		unsigned char *data = textures[i].get_data();
		int width = textures[i].get_width();
		int height = textures[i].get_height();
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glUseProgram(this->shader->program);
	glUniform1i(this->shader->uniforms["u_skybox"].location, 0);
}

void RAGE_material_skybox::use_shader()
{
	glUseProgram(this->shader->program);
}

RAGE_material_skybox::~RAGE_material_skybox()
{
	glDeleteTextures(1, &this->cube_sampler_id);
}