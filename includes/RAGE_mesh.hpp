#pragma once

#include <vector>

#include "RAGE_primitive.hpp"
#include "RAGE_material.hpp"

class RAGE_mesh
{
public:
	RAGE_mesh(RAGE_material *material = NULL, std::string name = "New Mesh");
	~RAGE_mesh();
	std::string name;
	std::vector<RAGE_primitive *> primitives;
	RAGE_material *material;
private:
};