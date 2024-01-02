#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>

class RAGE_transform
{
public:
	RAGE_transform(const glm::vec3 &position = glm::vec3(0.0f), const glm::vec3 &rotation = glm::vec3(0.0f), const glm::vec3 &scale = glm::vec3(1.0f, 1.0f, 1.0f));

	glm::mat4 get_model_matrix(bool update = false);
	void update_model_matrix();

	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;
protected:
	glm::quat quaternion;
	glm::mat4 model_matrix;

private:
};