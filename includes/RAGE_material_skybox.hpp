#pragma once

#include "RAGE_material.hpp"
#include "RAGE_texture2D.hpp"

class RAGE_material_skybox : public RAGE_material
{
public:
	RAGE_material_skybox();
	~RAGE_material_skybox();

	bool load_texture(const char *path, int index);
private:
	GLuint cube_sampler_id;
	RAGE_texture2D textures[6];
};