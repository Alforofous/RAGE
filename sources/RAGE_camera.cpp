#include "RAGE.hpp"

RAGE_camera::RAGE_camera()
{
	glm::vec3 initial_rotation = glm::vec3(0.0f, -90.0f, 0.0f);
	SetRotation(initial_rotation);
	m_position = glm::vec3(0.0f, 0.0f, 3.0f);
	m_forward = glm::vec3(0.0f, 0.0f, -1.0f);
	m_up = glm::vec3(0.0f, 1.0f, 0.0f);
}

void RAGE_camera::move_localy(glm::vec3 move)
{
	m_position += move;
}

void RAGE_camera::update_view()
{
	m_view = glm::lookAt(m_position, m_position + m_forward, m_up);
}
