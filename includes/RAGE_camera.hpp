#pragma once

#include "RAGE_object.hpp"
#include "RAGE_user_input.hpp"

class RAGE_camera : public RAGE_object
{
public:
	bool init(RAGE_window *window);
	glm::mat4 get_perspective_matrix();
	glm::mat4 get_view_matrix(bool update = true);
	glm::vec3 get_forward();
	glm::vec3 get_up();
	glm::vec3 get_right();
	void handle_input(RAGE_user_input *user_input, float delta_time);
private:
	void handle_movement(RAGE_user_input *user_input, float movement_speed);
	void handle_rotation(RAGE_user_input *user_input, float rotation_speed);

	void rotate_on_spherical_coordinates(glm::vec2 delta_movement);

	glm::mat4 m_perspective_matrix;
	glm::mat4 m_view_matrix;

	glm::vec3 m_forward;
	glm::vec3 m_up;
	glm::vec3 m_right;

	float m_movement_speed;
	float m_rotation_speed;
	float m_aspect_ratio;
	float m_fov;
};

glm::vec3 direction_to_euler(glm::vec3 direction);