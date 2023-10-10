#include "RAGE.hpp"
#include "nanogui/nanogui.h"

using namespace nanogui;

Screen *nanogui_screen = NULL;

static void error_callback(int error, const char *description)
{
	fprintf(stderr, "Error: %s\n", description);
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int modifiers)
{
	RAGE	*rage = (RAGE *)glfwGetWindowUserPointer(window);
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
		rage->camera->rotating_mode = true;

	nanogui_screen->mouseButtonCallbackEvent(button, action, modifiers);
}

static void cursor_position_callback(GLFWwindow *window, double x, double y)
{
	RAGE	*rage = (RAGE *)glfwGetWindowUserPointer(window);
//	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

	nanogui_screen->cursorPosCallbackEvent(x, y);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	RAGE	*rage = (RAGE *)glfwGetWindowUserPointer(window);
	if (key == GLFW_KEY_W && action == GLFW_PRESS)
		rage->camera->MoveLocaly(glm::vec3(0.0f, 0.0f, 1.0f));
	if (key == GLFW_KEY_S && action == GLFW_PRESS)
		rage->camera->MoveLocaly(glm::vec3(0.0f, 0.0f, -1.0f));

	nanogui_screen->keyCallbackEvent(key, scancode, action, mods);
}

int main(void)
{
	RAGE	*rage;
	if (glfwInit() == GLFW_FALSE)
		return (1);
	rage = new RAGE();
	if (rage->window->Init() == -1)
		return (1);

	/*Load GL and setup GLFW*/
	glfwSetErrorCallback(error_callback);
	glfwSetWindowUserPointer(rage->window->glfw_window, rage);
	glfwSetKeyCallback(rage->window->glfw_window, key_callback);
	glfwSetCursorPosCallback(rage->window->glfw_window, cursor_position_callback);
	glfwSetMouseButtonCallback(rage->window->glfw_window, mouse_button_callback);
	glfwMakeContextCurrent(rage->window->glfw_window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	/*Load shaders*/
	ShaderLoader shaderLoader("../shaders/vertex_test.glsl", "../shaders/fragment_test.glsl");

	/*Load viewport and start using shader program*/

	nanogui_screen = new Screen();
	nanogui_screen->initialize(rage->window->glfw_window, true);

	bool bvar = true;

	bool enabled = true;
	FormHelper *gui = new FormHelper(nanogui_screen);
	ref<Window> nanoguiWindow = gui->addWindow(Eigen::Vector2i(10, 10), "Form helper example");
	gui->addGroup("Basic types");
	gui->addVariable("bool", bvar)->setTooltip("Test tooltip.");

	nanogui_screen->setVisible(true);
	nanogui_screen->performLayout();
	nanoguiWindow->center();

	glfwSetCharCallback(rage->window->glfw_window,
		[](GLFWwindow *, unsigned int codepoint) {
			nanogui_screen->charCallbackEvent(codepoint);
		}
	);

	glfwSetDropCallback(rage->window->glfw_window,
		[](GLFWwindow *, int count, const char **filenames) {
			nanogui_screen->dropCallbackEvent(count, filenames);
		}
	);

	glfwSetScrollCallback(rage->window->glfw_window,
		[](GLFWwindow *, double x, double y) {
			nanogui_screen->scrollCallbackEvent(x, y);
		}
	);

	glfwSetFramebufferSizeCallback(rage->window->glfw_window,
		[](GLFWwindow *, int width, int height) {
			nanogui_screen->resizeCallbackEvent(width, height);
		}
	);

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
		nanogui_screen->drawContents();
		nanogui_screen->drawWidgets();
		glfwSwapBuffers(rage->window->glfw_window);
	}
	delete rage;
	return (0);
}
