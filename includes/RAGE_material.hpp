#pragma once

#include "glad/glad.h"
#include "RAGE_shader.hpp"

class RAGE_material
{
public:
	RAGE_material();
	~RAGE_material();

	bool load_shader(const char *vertex_shader_path, const char *fragment_shader_path);
protected:
	RAGE_shader *shader;
private:
};