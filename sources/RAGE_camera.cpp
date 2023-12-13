#include "RAGE.hpp"
#include "RAGE_camera.hpp"

RAGE_camera::RAGE_camera(float fov, glm::vec2 window_size, glm::vec3 position, glm::vec3 rotation, glm::vec2 z_plane)
{
	m_position = position;
	m_forward = glm::vec3(0.0f, 0.0f, -1.0f);
	m_up = glm::vec3(0.0f, 1.0f, 0.0f);
	m_right = glm::vec3(1.0f, 0.0f, 0.0f);
	m_fov = fov;
	m_aspect_ratio = window_size.x / window_size.y;
	m_rotation = rotation;
	m_view = glm::lookAt(m_position, m_position + m_forward, m_up);
	m_projection_matrix = glm::perspective(m_fov, m_aspect_ratio, z_plane.x, z_plane.y);
}

void RAGE_camera::update_view()
{
	m_view = glm::lookAt(m_position, m_position + m_forward, m_up);
}
