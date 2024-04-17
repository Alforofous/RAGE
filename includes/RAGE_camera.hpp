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
	glm::vec2 get_clip_planes();
	void handle_input(RAGE_user_input *user_input, float delta_time);
	void set_aspect_ratio(glm::ivec2 window_size);
	glm::mat4 m_perspective_matrix;
	glm::mat4 m_view_matrix;

private:
	void handle_movement(RAGE_user_input *user_input, float movement_speed);
	void handle_rotation(RAGE_user_input *user_input, float rotation_speed);

	void rotate_on_spherical_coordinates(glm::vec2 delta_movement);

	glm::vec3 m_forward;
	glm::vec3 m_up;
	glm::vec3 m_right;

	float m_movement_speed;
	float rotation_speed;
	float m_aspect_ratio;
	float m_fov;
	glm::vec2 clip_planes;
};

glm::vec3 direction_to_euler(glm::vec3 direction);