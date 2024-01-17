#include "RAGE_material.hpp"

RAGE_material::RAGE_material()
{
	this->shader = NULL;
}

bool RAGE_material::load_shader(const char *vertex_shader_path, const char *fragment_shader_path)
{
	this->shader = new RAGE_shader(vertex_shader_path, fragment_shader_path);
	return (true);
}

RAGE_material::~RAGE_material()
{
	if (this->shader != NULL)
		delete this->shader;
}