#include "RAGE_transform.hpp"

RAGE_transform::RAGE_transform(const glm::vec3 &position, const glm::vec3 &rotation, const glm::vec3 &scale)
{
	m_position = position;
	m_rotation = rotation;
	m_scale = scale;
}

glm::mat4 RAGE_transform::get_model_matrix(bool update)
{
	if (update)
		this->update_model_matrix();
	return (this->model_matrix);
}

void RAGE_transform::update_model_matrix()
{
	glm::mat4 pos_matrix = glm::translate(m_position);
	
	glm::quat rot_x = glm::angleAxis(glm::radians(m_rotation.x), glm::vec3(1, 0, 0));
	glm::quat rot_y = glm::angleAxis(glm::radians(m_rotation.y), glm::vec3(0, 1, 0));
	glm::quat rot_z = glm::angleAxis(glm::radians(m_rotation.z), glm::vec3(0, 0, 1));
	
	glm::quat quaternion = rot_y * rot_x * rot_z;

	glm::mat4 rot_matrix = glm::mat4_cast(quaternion);
	glm::mat4 scale_matrix = glm::scale(m_scale);

	this->model_matrix = pos_matrix * rot_matrix * scale_matrix;
}

void RAGE_transform::translate(const glm::vec3 &move) { m_position += move; }
void RAGE_transform::rotate(const glm::vec3 &rotate) { m_rotation += rotate; }
void RAGE_transform::scale(const glm::vec3 &scale) { m_scale += scale; }

glm::vec3 &RAGE_transform::get_position() { return m_position; }
glm::vec3 &RAGE_transform::get_rotation() { return m_rotation; }
glm::vec3 &RAGE_transform::get_scale() { return m_scale; }

void RAGE_transform::set_position(glm::vec3 &pos) { m_position = pos; }
void RAGE_transform::set_rotation(glm::vec3 &rot) { m_rotation = rot; }
void RAGE_transform::set_scale(glm::vec3 &scale) { m_scale = scale; }
