#include "RAGE_mesh.hpp"

RAGE_mesh::RAGE_mesh(RAGE_material *material, std::string name)
{
	this->material = material;
	this->name = name;
}

RAGE_mesh::~RAGE_mesh()
{
	if (this->material != NULL)
		delete this->material;
}