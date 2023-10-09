#pragma once

#include "RAGE_object.hpp"

class RAGE_camera : public RAGE_object
{
public:
	RAGE_camera();
	void MoveLocaly(glm::vec3 move);
	bool rotating_mode;
private:
	glm::vec3	m_forward;
	glm::vec3	m_up;
	glm::vec3	m_right;
};