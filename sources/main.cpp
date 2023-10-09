#include "RAGE.hpp"
#include "main.h"

static void error_callback(int error, const char *description)
{
	fprintf(stderr, "Error: %s\n", description);
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
	RAGE	*rage = (RAGE *)glfwGetWindowUserPointer(window);
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
		rage->camera->rotating_mode = true;
}

static void cursor_position_callback(GLFWwindow *window, double xpos, double ypos)
{
	RAGE	*rage = (RAGE *)glfwGetWindowUserPointer(window);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	RAGE	*rage = (RAGE *)glfwGetWindowUserPointer(window);
	if (key == GLFW_KEY_W && action == GLFW_PRESS)
		rage->camera->MoveLocaly(glm::vec3(0.0f, 0.0f, 1.0f));
	if (key == GLFW_KEY_S && action == GLFW_PRESS)
		rage->camera->MoveLocaly(glm::vec3(0.0f, 0.0f, -1.0f));
}

int main(void)
{
	RAGE	*rage = new RAGE();
	if (glfwInit() == GLFW_FALSE)
		return (1);
	if (rage->window->Init() == -1)
		return (1);

	/*Load GL and setup GLFW*/
	glfwSetErrorCallback(error_callback);
	glfwSetWindowUserPointer(rage->window->glfw_window, rage);
	glfwSetKeyCallback(rage->window->glfw_window, key_callback);
	glfwSetCursorPosCallback(rage->window->glfw_window, cursor_position_callback);
	glfwSetMouseButtonCallback(rage->window->glfw_window, mouse_button_callback);
	glfwMakeContextCurrent(rage->window->glfw_window);
	gladLoadGL();

	/*Load shaders*/
	ShaderLoader shaderLoader("../shaders/vertex_test.glsl", "../shaders/fragment_test.glsl");

	/*Load viewport and start using shader program*/
	glViewport(0, 0, rage->window->mode->width, rage->window->mode->height);
	glUseProgram(shaderLoader.hProgram);

	/*Set GLSL variable locations*/
	int resolutionLocation = glGetUniformLocation(shaderLoader.hProgram, "u_resolution");
	int cameraPositionLocation = glGetUniformLocation(shaderLoader.hProgram, "u_camera_position");
	int cameraDirectionLocation = glGetUniformLocation(shaderLoader.hProgram, "u_camera_direction");

	int width;
	int height;
	rage->camera->SetPosition(glm::vec3(0.0, 0.0, -3.0));
	while (!glfwWindowShouldClose(rage->window->glfw_window))
	{
		glfwGetWindowSize(rage->window->glfw_window, &width, &height);
		glm::vec3 camera_rotation = rage->camera->GetRotation();
		if (rage->camera->rotating_mode == true)
			 rage->camera->SetRotation(camera_rotation + glm::vec3(0.1f, 0.0f, 0.0f));
		glm::vec3 camera_direction;
		camera_direction.x = cos(glm::radians(camera_rotation.y)) * cos(glm::radians(camera_rotation.x));
		camera_direction.y = sin(glm::radians(camera_rotation.x));
		camera_direction.z = sin(glm::radians(camera_rotation.y)) * cos(glm::radians(camera_rotation.x));
		camera_direction = glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 camera_position = rage->camera->GetPosition();
		glUniform2f(resolutionLocation, (float)width, (float)height);
		glUniform3f(cameraPositionLocation, camera_position.x, camera_position.y, camera_position.z);
		glUniform3f(cameraDirectionLocation, camera_direction.x, camera_direction.y, camera_direction.z);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glfwPollEvents();
		glfwSwapBuffers(rage->window->glfw_window);
	}
	delete rage;
	return (0);
}
