#pragma once

#include <glm/glm.hpp>
#include "RAGE_object.hpp"
#include "RAGE_primitive_objects.hpp"
#include "RAGE_geometry.hpp"

struct axis_aligned_bounding_box
{
	glm::vec3 min;
	glm::vec3 max;
};

struct bounding_box_node
{
	axis_aligned_bounding_box bounding_box;
	bounding_box_node *left;
	bounding_box_node *right;
	RAGE_object *object;
};

struct RAGE_ray_hit_info
{
	glm::vec3 position;
	glm::vec3 normal;
	RAGE_object *object;
};

struct RAGE_ray
{
	glm::vec3 origin;
	glm::vec3 direction;
	RAGE_ray_hit_info hit_info;
};

class RAGE_ray_tracing
{
public:
	RAGE_ray_tracing();

	RAGE_ray ray;
};

class RAGE_bounding_volume_hierarchy
{
public:
	RAGE_bounding_volume_hierarchy();

	void build();
	void draw();
	std::vector<RAGE_object *> objects;
	std::vector<RAGE_object *> bounding_box_objects;
private:
	bounding_box_node *root;
	RAGE_geometry *geometry;
};