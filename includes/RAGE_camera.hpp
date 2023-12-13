#pragma once

#include "RAGE_object.hpp"

class RAGE_camera : public RAGE_object
{
public:
	RAGE_camera(float fov = 45.0f, glm::vec2 window_size = glm::vec2(800, 600), glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 rotation = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec2 z_plane = glm::vec2(0.1f, 100.0f));
	glm::mat4 m_view;
	void move_localy(glm::vec3 move);
	void update_view();
	bool rotating_mode;

private:
	glm::vec3 m_forward;
	glm::vec3 m_up;
	glm::vec3 m_right;
	float m_aspect_ratio;
	float m_fov;
	glm::mat4 m_projection_matrix;
};