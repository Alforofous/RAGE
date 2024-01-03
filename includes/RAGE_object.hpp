#pragma once

#include "RAGE_transform.hpp"
#include "RAGE_mesh.hpp"
#include "RAGE_primitive.hpp"
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
	static void draw_objects(RAGE_object **objects, size_t count);
	void draw();
	static void print_name(RAGE_object *object);
	static void delete_object(RAGE_object *object);

	polygon_mode polygon_mode;
	std::string name;
	std::vector<unsigned int> children_indices;
private:
	RAGE_mesh *mesh;
};