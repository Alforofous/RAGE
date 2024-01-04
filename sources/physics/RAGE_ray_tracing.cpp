#include "physics/RAGE_ray_tracing.hpp"

RAGE_ray_tracing::RAGE_ray_tracing()
{
	ray.origin = glm::vec3(0.0f, 0.0f, 0.0f);
	ray.direction = glm::vec3(0.0f, 0.0f, -1.0f);
}

RAGE_bounding_volume_hierarchy::RAGE_bounding_volume_hierarchy()
{
	this->root = NULL;
	this->primitive = NULL;
}

void RAGE_bounding_volume_hierarchy::build()
{
	for (size_t count = 0; count < this->bounding_box_objects.size(); count++)
	{
		RAGE_object *object = this->bounding_box_objects[count];
		delete object;
	}
	this->bounding_box_objects.clear();
	for (size_t count = 0; count < this->objects.size(); count++)
	{
	}
}

void RAGE_bounding_volume_hierarchy::draw()
{
	for (size_t count = 0; count < this->bounding_box_objects.size(); count++)
	{
		RAGE_object *object = this->bounding_box_objects[count];
	}
}