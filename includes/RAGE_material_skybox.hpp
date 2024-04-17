#pragma once

#include "RAGE_material.hpp"
#include "RAGE_texture2D.hpp"

class RAGE_material_skybox : public RAGE_material
{
public:
	RAGE_material_skybox();
	~RAGE_material_skybox();

	void use_shader();

private:
	GLuint cube_sampler_id;
};