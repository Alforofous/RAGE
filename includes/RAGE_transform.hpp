#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>

class RAGE_transform
{
public:
	RAGE_transform(const glm::vec3 &position = glm::vec3(), const glm::vec3 &rotation = glm::vec3(), const glm::vec3 &scale = glm::vec3(1.0f, 1.0f, 1.0f));

	glm::mat4 GetModelMatrix() const;
	void translate(const glm::vec3 &move);
	void rotate(const glm::vec3 &rotate);
	void scale(const glm::vec3 &scale);
	
	glm::vec3 &GetPosition();
	glm::vec3 &GetRotation();
	glm::vec3 &GetScale();

	void SetPosition(glm::vec3 &pos);
	void SetRotation(glm::vec3 &rot);
	void SetScale(glm::vec3 &scale);

protected:
	glm::vec3 m_position;
	glm::vec3 m_rotation;
	glm::vec3 m_scale;
	glm::quat quaternion;
};