#include "RAGE.hpp"
#include "RAGE_camera.hpp"

bool RAGE_camera::init(RAGE_window *window)
{
	m_position = glm::vec3(0.0f, 0.0f, 3.0f);
	m_forward = glm::vec3(0.0f, 0.0f, -1.0f);
	m_up = glm::vec3(0.0f, 1.0f, 0.0f);
	m_right = glm::vec3(1.0f, 0.0f, 0.0f);
	m_fov = 45.0f;
	m_aspect_ratio = static_cast<float>(window->get_pixel_size().x) / window->get_pixel_size().y;
	m_rotation = direction_to_euler(m_forward);
	m_view_matrix = RAGE_camera::get_view_matrix();
	m_perspective_matrix = glm::perspective(glm::radians(m_fov), m_aspect_ratio, 0.1f, 100.0f);
	return (true);
}

void RAGE_camera::handle_input(RAGE_user_input *user_input, float delta_time)
{
	m_movement_speed = 0.01f;
	float movement_speed = m_movement_speed * delta_time;
	handle_movement(user_input, movement_speed);

	m_rotation_speed = 0.002f * delta_time;
	handle_rotation(user_input, m_rotation_speed);
}

glm::vec3 direction_to_euler(glm::vec3 direction)
{
	direction = glm::normalize(direction);

	float pitch = asin(direction.y);
	float yaw = atan2(direction.z, direction.x);

	return glm::vec3(pitch, yaw, 0.0f);
}

glm::vec3 euler_to_direction(glm::vec3 euler)
{
	glm::vec3 direction;
	direction.x = cos(euler.y) * cos(euler.x);
	direction.y = sin(euler.x);
	direction.z = sin(euler.y) * cos(euler.x);
	return glm::normalize(direction);
}

void RAGE_camera::handle_movement(RAGE_user_input *user_input, float movement_speed)
{
	glm::vec3 new_up = glm::cross(this->get_right(), this->get_forward());
	if (user_input->keyboard.pressed_keys[GLFW_KEY_W])
		this->translate(this->get_forward() * movement_speed);
	if (user_input->keyboard.pressed_keys[GLFW_KEY_S])
		this->translate(-this->get_forward() * movement_speed);
	if (user_input->keyboard.pressed_keys[GLFW_KEY_A])
		this->translate(-this->get_right() * movement_speed);
	if (user_input->keyboard.pressed_keys[GLFW_KEY_D])
		this->translate(this->get_right() * movement_speed);
	if (user_input->keyboard.pressed_keys[GLFW_KEY_E])
		this->translate(new_up * movement_speed);
	if (user_input->keyboard.pressed_keys[GLFW_KEY_Q])
		this->translate(-new_up * movement_speed);
}

void RAGE_camera::handle_rotation(RAGE_user_input *user_input, float rotation_speed)
{
	glm::vec2 delta_movement(0.0f, 0.0f);
	if (user_input->keyboard.pressed_keys[GLFW_KEY_UP])
		delta_movement.y += rotation_speed;
	if (user_input->keyboard.pressed_keys[GLFW_KEY_DOWN])
		delta_movement.y -= rotation_speed;
	if (user_input->keyboard.pressed_keys[GLFW_KEY_LEFT])
		delta_movement.x -= rotation_speed;
	if (user_input->keyboard.pressed_keys[GLFW_KEY_RIGHT])
		delta_movement.x += rotation_speed;
	rotate_on_spherical_coordinates(delta_movement);
}

void RAGE_camera::rotate_on_spherical_coordinates(glm::vec2 delta_movement)
{
	m_rotation.x += delta_movement.y;
	m_rotation.y += delta_movement.x;
	m_rotation.x = glm::clamp(m_rotation.x, -glm::half_pi<float>() + 0.001f, glm::half_pi<float>() - 0.001f);

	m_forward = euler_to_direction(m_rotation);

	m_forward = glm::normalize(m_forward);
	m_right = glm::normalize(glm::cross(m_forward, this->get_up()));
}

glm::mat4 RAGE_camera::get_perspective_matrix()
{
	return (m_perspective_matrix);
}

glm::mat4 RAGE_camera::get_view_matrix(bool update)
{
	if (update)
	{
		m_view_matrix = glm::lookAt(m_position, m_position + m_forward, m_up);
	}
	return (m_view_matrix);
}

glm::vec3 RAGE_camera::get_forward() { return (m_forward); }
glm::vec3 RAGE_camera::get_up() { return (m_up); }
glm::vec3 RAGE_camera::get_right() { return (m_right); }