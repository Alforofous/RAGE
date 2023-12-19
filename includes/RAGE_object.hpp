#pragma once

#include "RAGE_transform.hpp"
#include "RAGE_mesh.hpp"
#include "GLobject.hpp"
#include <vector>

class RAGE_object : public RAGE_transform
{
public:
	RAGE_object();
	~RAGE_object();
	bool load_GLB_mesh(const char *path);
	bool has_mesh();
private:
	RAGE_mesh *mesh;
	GLobject gl_object;
};