#pragma once

#include "RAGE_transform.hpp"
#include "RAGE_mesh.hpp"
#include "GLobject.hpp"
#include <vector>

class RAGE;

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
	static void draw_objects(RAGE_object **objects, size_t count);
	void draw();

private:
	int u_model_matrix_variable_location;
	std::string name;
	RAGE_mesh *mesh;
	GLobject gl_object;
};