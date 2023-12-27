#include "physics/RAGE_ray_tracing.hpp"

RAGE_ray_tracing::RAGE_ray_tracing()
{
	ray.origin = glm::vec3(0.0f, 0.0f, 0.0f);
	ray.direction = glm::vec3(0.0f, 0.0f, -1.0f);
}

RAGE_bounding_volume_hierarchy::RAGE_bounding_volume_hierarchy()
{
	this->root = NULL;
	this->gl_object = NULL;
}

void RAGE_bounding_volume_hierarchy::build()
{
	for (size_t count = 0; count < this->objects.size(); count++)
	{
		RAGE_object *object = this->objects[count];
		if (object->has_mesh() == false)
			continue;
		RAGE_mesh *mesh = object->get_mesh();
		if (mesh->vertex_positions.size() == 0)
			continue;
		std::vector<float> vertex_positions = mesh->vertex_positions;
		glm::vec3 min = glm::vec3(vertex_positions[0], vertex_positions[1], vertex_positions[2]);
		glm::vec3 max = glm::vec3(vertex_positions[0], vertex_positions[1], vertex_positions[2]);
		for (size_t count = 3; count < vertex_positions.size(); count++)
		{
			glm::vec3 vertex_position = glm::vec3(vertex_positions[count + 0],
												  vertex_positions[count + 1],
												  vertex_positions[count + 2]);
			if (vertex_position.x < min.x)
				min.x = vertex_position.x;
			if (vertex_position.y < min.y)
				min.y = vertex_position.y;
			if (vertex_position.z < min.z)
				min.z = vertex_position.z;
			if (vertex_position.x > max.x)
				max.x = vertex_position.x;
			if (vertex_position.y > max.y)
				max.y = vertex_position.y;
			if (vertex_position.z > max.z)
				max.z = vertex_position.z;
		}
		bounding_box_node *node = new bounding_box_node();
		node->bounding_box.min = min;
		node->bounding_box.max = max;
		node->object = object;
		this->root = node;
		RAGE_object *bounding_box_object = RAGE_primitive_objects::create_cube();
		bounding_box_object->scale(glm::vec3(max.x - min.x, max.y - min.y, max.z - min.z));
		bounding_box_object->translate(glm::vec3((max.x + min.x) / 2.0f, (max.y + min.y) / 2.0f, (max.z + min.z) / 2.0f));
		bounding_box_object->update_model_matrix();
		this->bounding_box_objects.push_back(bounding_box_object);

	}
}

void RAGE_bounding_volume_hierarchy::draw()
{
	for (size_t count = 0; count < this->bounding_box_objects.size(); count++)
	{
		RAGE_object *object = this->bounding_box_objects[count];
	}
}