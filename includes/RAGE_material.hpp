#pragma once

#include "glad/glad.h"
#include "RAGE_shader.hpp"
#include "RAGE_texture2D.hpp"
#include <vector>

class RAGE_material
{
public:
	RAGE_material();

	void use_shader();
	GLint get_shader_location();
	std::string name;
	std::vector<RAGE_texture2D> &get_textures();
protected:
	RAGE_shader *shader;
	std::vector<RAGE_texture2D> textures;
private:
};