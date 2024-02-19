#pragma once

#include "glad/glad.h"
#include "RAGE_shader.hpp"

class RAGE_material
{
public:
	RAGE_material();

	void use_shader();
	GLint get_shader_location();
	std::string name;
protected:
	RAGE_shader *shader;
private:
};