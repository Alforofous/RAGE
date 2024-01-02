#include "RAGE_transform.hpp"

RAGE_transform::RAGE_transform(const glm::vec3 &position, const glm::vec3 &rotation, const glm::vec3 &scale)
{
	this->position = position;
	this->rotation = rotation;
	this->scale = scale;
}

glm::mat4 RAGE_transform::get_model_matrix(bool update)
{
	if (update)
		this->update_model_matrix();
	return (this->model_matrix);
}

void RAGE_transform::update_model_matrix()
{
	glm::mat4 pos_matrix = glm::translate(position);
	
	glm::quat rot_x = glm::angleAxis(glm::radians(rotation.x), glm::vec3(1, 0, 0));
	glm::quat rot_y = glm::angleAxis(glm::radians(rotation.y), glm::vec3(0, 1, 0));
	glm::quat rot_z = glm::angleAxis(glm::radians(rotation.z), glm::vec3(0, 0, 1));
	
	glm::quat quaternion = rot_y * rot_x * rot_z;

	glm::mat4 rot_matrix = glm::mat4_cast(quaternion);
	glm::mat4 scale_matrix = glm::scale(scale);

	this->model_matrix = pos_matrix * rot_matrix * scale_matrix;
}