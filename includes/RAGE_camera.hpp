#pragma once

#include "RAGE_object.hpp"

class RAGE_camera : public RAGE_object
{
public:
	glm::mat4	m_view;
	RAGE_camera();
	void move_localy(glm::vec3 move);
	void update_view();
	bool rotating_mode;
private:
	glm::vec3	m_forward;
	glm::vec3	m_up;
	glm::vec3	m_right;
};