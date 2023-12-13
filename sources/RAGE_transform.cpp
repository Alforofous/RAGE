#include "RAGE_transform.hpp"

RAGE_transform::RAGE_transform(const glm::vec3 &position, const glm::vec3 &rotation, const glm::vec3 &scale)
{
	m_position = position;
	m_rotation = rotation;
	m_scale = scale;
}

glm::mat4 RAGE_transform::GetModelMatrix() const
{
	glm::mat4 posMatrix = glm::translate(m_position);
	glm::mat4 rotXMatrix = glm::rotate(m_rotation.x, glm::vec3(1, 0, 0));
	glm::mat4 rotYMatrix = glm::rotate(m_rotation.y, glm::vec3(0, 1, 0));
	glm::mat4 rotZMatrix = glm::rotate(m_rotation.z, glm::vec3(1, 0, 0));
	glm::mat4 scaleMatrix = glm::scale(m_scale);

	glm::mat4 rotMatrix = rotZMatrix * rotYMatrix * rotXMatrix;

	return posMatrix * rotMatrix * scaleMatrix;
}

void RAGE_transform::translate(glm::vec3 &move) { m_position += move; }

glm::vec3 &RAGE_transform::GetPosition() { return m_position; }
glm::vec3 &RAGE_transform::GetRotation() { return m_rotation; }
glm::vec3 &RAGE_transform::GetScale() { return m_scale; }

void RAGE_transform::SetPosition(glm::vec3 &pos) { m_position = pos; }
void RAGE_transform::SetRotation(glm::vec3 &rot) { m_rotation = rot; }
void RAGE_transform::SetScale(glm::vec3 &scale) { m_scale = scale; }