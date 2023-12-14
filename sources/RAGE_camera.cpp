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
	m_view_matrix = RAGE_camera::get_view_matrix();
	m_perspective_matrix = glm::perspective(m_fov, m_aspect_ratio, z_plane.x, z_plane.y);
}

void RAGE_camera::handle_input(RAGE_user_input *user_input, float delta_time)
{
	m_movement_speed = 0.01f;
	if (user_input->keyboard.pressed_keys[GLFW_KEY_W])
		this->translate(this->get_forward() * m_movement_speed * delta_time);
	if (user_input->keyboard.pressed_keys[GLFW_KEY_S])
		this->translate(-this->get_forward() * m_movement_speed * delta_time);
	if (user_input->keyboard.pressed_keys[GLFW_KEY_A])
		this->translate(-this->get_right() * m_movement_speed * delta_time);
	if (user_input->keyboard.pressed_keys[GLFW_KEY_D])
		this->translate(this->get_right() * m_movement_speed * delta_time);
	if (user_input->keyboard.pressed_keys[GLFW_KEY_E])
		this->translate(this->get_up() * m_movement_speed * delta_time);
	if (user_input->keyboard.pressed_keys[GLFW_KEY_Q])
		this->translate(-this->get_up() * m_movement_speed * delta_time);
}

glm::mat4 RAGE_camera::get_perspective_matrix()
{
	return (m_perspective_matrix);
}

glm::mat4 RAGE_camera::get_view_matrix(bool update)
{
	if (update)
		m_view_matrix = glm::lookAt(m_position, m_position + m_forward, m_up);
	return (m_view_matrix);
}

glm::vec3 RAGE_camera::get_forward() { return (m_forward); }
glm::vec3 RAGE_camera::get_up() { return (m_up); }
glm::vec3 RAGE_camera::get_right() { return (m_right); }