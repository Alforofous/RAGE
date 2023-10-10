#include "RAGE.hpp"

//This function is meant to be called every frame to set new values to GLSL shader
void set_shader_variable_values(void *content)
{
	int		width;
	int		height;
	RAGE	*rage;

	rage = (RAGE *)content;
	glfwGetWindowSize(rage->window->glfw_window, &width, &height);
	glUniform2f(rage->shader->variable_location["u_resolution"], (float)width, (float)height);

	glm::vec3 camera_position = rage->camera->GetPosition();
	glUniform3f(rage->shader->variable_location["u_camera_position"], camera_position.x, camera_position.y, camera_position.z);

	glm::vec3 camera_rotation = rage->camera->GetRotation();

	glm::vec3 camera_direction;
	camera_direction.x = cos(glm::radians(camera_rotation.y)) * cos(glm::radians(camera_rotation.x));
	camera_direction.y = sin(glm::radians(camera_rotation.x));
	camera_direction.z = sin(glm::radians(camera_rotation.y)) * cos(glm::radians(camera_rotation.x));
	camera_direction = glm::vec3(0.0f, 0.0f, -1.0f);
	glUniform3f(rage->shader->variable_location["u_camera_direction"], camera_direction.x, camera_direction.y, camera_direction.z);
}
