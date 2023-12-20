#pragma once

#include "RAGE_transform.hpp"
#include "RAGE_mesh.hpp"
#include "GLobject.hpp"
#include <vector>

class RAGE_object : public RAGE_transform
{
public:
	RAGE_object(RAGE_mesh *mesh = NULL, const char *name = "New Object");
	~RAGE_object();
	bool init();
	bool load_GLB_mesh(const char *path);
	bool has_mesh();
	RAGE_mesh *get_mesh();
	std::string get_name();
	GLobject *get_gl_object();
private:
	std::string name;
	RAGE_mesh *mesh;
	GLobject gl_object;
};