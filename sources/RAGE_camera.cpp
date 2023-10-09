#include "RAGE.hpp"

RAGE_camera::RAGE_camera()
{
	SetRotation(glm::vec3(0.0f, -90.0f, 0.0f));
}

void RAGE_camera::MoveLocaly(glm::vec3 move)
{
	m_position += move;
}
