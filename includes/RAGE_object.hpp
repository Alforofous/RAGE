#pragma once

#include "RAGE_transform.hpp"
#include "RAGE_mesh.hpp"
#include "RAGE_geometry.hpp"
#include <vector>

enum polygon_mode
{
	fill,
	line,
	point
};

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
	RAGE_geometry *get_geometry();
	static void draw_objects(RAGE_object **objects, size_t count);
	static void init_objects(RAGE_object **objects, size_t count);
	void draw();

	polygon_mode polygon_mode;
	std::string name;
	std::vector<RAGE_object *> children;
private:
	int u_model_matrix_variable_location;
	RAGE_mesh *mesh;
	RAGE_geometry geometry;
};