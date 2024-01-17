#pragma once

#include "RAGE_material.hpp"
#include "RAGE_texture2D.hpp"

class RAGE_skybox_material : public RAGE_material
{
public:
	RAGE_skybox_material();
	~RAGE_skybox_material();

	bool load_texture(const char *path, int index);
private:
	GLuint cube_sampler_id;
	RAGE_texture2D textures[6];
};