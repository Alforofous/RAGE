#include "RAGE.hpp"

int main(void)
{
	RAGE	*rage;

	rage = new RAGE();
	if (glfwInit() == GLFW_FALSE)
		return (1);
	if (rage->window->Init() == -1)
		return (1);

	set_callbacks(rage);
	glfwMakeContextCurrent(rage->window->glfw_window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	ShaderLoader shaderLoader("../shaders/vertex_test.glsl", "../shaders/fragment_test.glsl");

	rage->nanogui_screen = new Screen();
	rage->nanogui_screen->initialize(rage->window->glfw_window, true);

	bool bvar = true;
	FormHelper *gui = new FormHelper(rage->nanogui_screen);
	ref<Window> nanoguiWindow = gui->addWindow(Eigen::Vector2i(10, 10), "Form helper example");
	gui->addGroup("Basic types");
	gui->addVariable("bool", bvar)->setTooltip("Test tooltip.");

	rage->nanogui_screen->setVisible(true);
	rage->nanogui_screen->performLayout();

	/*Set GLSL variable locations*/
	int resolutionLocation = glGetUniformLocation(shaderLoader.hProgram, "u_resolution");
	int cameraPositionLocation = glGetUniformLocation(shaderLoader.hProgram, "u_camera_position");
	int cameraDirectionLocation = glGetUniformLocation(shaderLoader.hProgram, "u_camera_direction");

	int width;
	int height;
	rage->camera->SetPosition(glm::vec3(0.0, 0.0, -3.0));
	while (!glfwWindowShouldClose(rage->window->glfw_window))
	{
		glfwPollEvents();
		glfwGetWindowSize(rage->window->glfw_window, &width, &height);
		glViewport(0, 0, width, height);
		glUseProgram(shaderLoader.hProgram);
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
		glClearColor(0.03f, 0.04f, 0.07f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		rage->nanogui_screen->drawContents();
		rage->nanogui_screen->drawWidgets();
		glfwSwapBuffers(rage->window->glfw_window);
	}
	return (0);
}
