#include "RAGE_material.hpp"
#include "RAGE.hpp"

RAGE_material::RAGE_material()
{
	RAGE *rage = get_rage();

	this->shader = rage->shader;
	this->name = "Default material";
}

void RAGE_material::use_shader()
{
	glUseProgram(this->shader->program);
}

GLint RAGE_material::get_shader_location()
{
	return (this->shader->program);
}