#include "RAGE.hpp"

int main(void)
{
	RAGE	*rage;

	rage = new RAGE();
	if (glfwInit() == GLFW_FALSE)
		return (1);
	if (rage->window->Init() == -1)
		return (1);

	glfwMakeContextCurrent(rage->window->glfw_window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	rage->gui = new RAGE_gui(rage->window->glfw_window);

	set_callbacks(rage);

	bool bvar = true;
	FormHelper *gui = new FormHelper(rage->gui->nano_screen);
	ref<Window> nanoguiWindow = gui->addWindow(Eigen::Vector2i(10, 10), "Form helper example");
	gui->addGroup("Basic types");
	gui->addVariable("bool", bvar)->setTooltip("Test tooltip.");

	rage->gui->nano_screen->setVisible(true);
	rage->gui->nano_screen->performLayout();

	/*Set GLSL variable locations*/
	rage->shader = new RAGE_shader("../shaders/vertex_test.glsl", "../shaders/fragment_test.glsl");
	rage->shader->InitVariableLocations();

	Widget	*widget = new Widget(rage->gui->nano_screen);
	TextBox	textBox(widget, "Test");
	textBox.setEnabled(true);

	int width;
	int height;
	rage->camera->SetPosition(glm::vec3(0.0, 0.0, -3.0));
	while (!glfwWindowShouldClose(rage->window->glfw_window))
	{
		glfwPollEvents();
		glfwGetWindowSize(rage->window->glfw_window, &width, &height);
		glViewport(0, 0, width, height);
		glUseProgram(rage->shader->hProgram);
		glm::vec3 camera_rotation = rage->camera->GetRotation();
		if (rage->camera->rotating_mode == true)
			 rage->camera->SetRotation(camera_rotation + glm::vec3(0.1f, 0.0f, 0.0f));
		set_shader_variable_values(rage);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		rage->gui->nano_screen->drawContents();
		rage->gui->nano_screen->drawWidgets();
		glfwSwapBuffers(rage->window->glfw_window);
	}
	return (0);
}
