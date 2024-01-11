#pragma once

#include "RAGE_transform.hpp"
#include "RAGE_mesh.hpp"
#include <vector>
#include <string>

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
	void draw();
	void remove_mesh();

	static void draw_objects(RAGE_object **objects, size_t count);
	static void delete_object(RAGE_object *object);

	polygon_mode polygon_mode;
	std::string name;
	std::vector<unsigned int> children_indices;
	RAGE_mesh *mesh;
private:
};