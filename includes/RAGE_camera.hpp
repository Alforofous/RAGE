#pragma once

#include "RAGE_object.hpp"
#include "RAGE_user_input.hpp"

class RAGE_camera : public RAGE_object
{
public:
	RAGE_camera(float fov = 45.0f, glm::vec2 window_size = glm::vec2(800, 600), glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 rotation = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec2 z_plane = glm::vec2(0.1f, 100.0f));
	glm::mat4 get_perspective_matrix();
	glm::mat4 get_view_matrix(bool update = true);
	glm::vec3 get_forward();
	glm::vec3 get_up();
	glm::vec3 get_right();
	void handle_input(RAGE_user_input *user_input, float delta_time);
private:
	void handle_movement(RAGE_user_input *user_input, float movement_speed);
	void handle_rotation(RAGE_user_input *user_input, float rotation_speed);

	void rotate_on_sperical_coordinates(glm::vec2 delta_movement);

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