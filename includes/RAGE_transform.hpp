#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

class Transform
{
public:
	Transform(const glm::vec3 &position = glm::vec3(), const glm::vec3 &rotation = glm::vec3(), const glm::vec3 &scale = glm::vec3(1.0f, 1.0f, 1.0f)) :

		m_position(position),
		m_rotation(rotation),
		m_scale(scale) {}

	inline glm::mat4 GetModelMatrix() const
	{
		glm::mat4 posMatrix = glm::translate(m_position);
		glm::mat4 rotXMatrix = glm::rotate(m_rotation.x, glm::vec3(1, 0, 0));
		glm::mat4 rotYMatrix = glm::rotate(m_rotation.y, glm::vec3(0, 1, 0));
		glm::mat4 rotZMatrix = glm::rotate(m_rotation.z, glm::vec3(1, 0, 0));
		glm::mat4 scaleMatrix = glm::scale(m_scale);

		glm::mat4 rotMatrix = rotZMatrix * rotYMatrix * rotXMatrix;

		return posMatrix * rotMatrix * scaleMatrix;
	}

	inline glm::vec3 &GetPosition() { return m_position; }
	inline glm::vec3 &GetRotation() { return m_rotation; }
	inline glm::vec3 &GetScale() { return m_scale; }

	inline void SetPosition(glm::vec3 &pos) { m_position = pos; }
	inline void SetRotation(glm::vec3 &rot) { m_rotation = rot; }
	inline void SetScale(glm::vec3 &scale) { m_scale = scale; }

protected:
	glm::vec3	m_position;
	glm::vec3	m_rotation;
	glm::vec3	m_scale;
private:
};