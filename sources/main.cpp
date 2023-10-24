#include "RAGE.hpp"

double clockToMilliseconds(clock_t ticks)
{
	return (ticks / (double)CLOCKS_PER_SEC) * 1000.0;
}

int main(void)
{
	RAGE	*rage;

	rage = new RAGE();
	if (glfwInit() == GLFW_FALSE)
		return (1);
	if (rage->window->Init() == -1)
		return (1);

	glfwMakeContextCurrent(rage->window->glfw_window);
	gladLoadGL();

	set_callbacks(rage);
	rage->gui = new RAGE_gui(rage->window->glfw_window);

	/*Set GLSL variable locations*/
	rage->shader = new RAGE_shader("../shaders/vertex_test.glsl", "../shaders/fragment_test.glsl");
	rage->shader->InitVariableLocations();

	rage->camera->SetPosition(glm::vec3(0.0, 0.0, -3.0));
	while (!glfwWindowShouldClose(rage->window->glfw_window))
	{
		clock_t beginFrame = clock();

		glfwPollEvents();
		glUseProgram(rage->shader->hProgram);
		glm::vec3 camera_rotation = rage->camera->GetRotation();
		if (rage->camera->rotating_mode == true)
			 rage->camera->SetRotation(camera_rotation + glm::vec3(0.1f, 0.0f, 0.0f));
		set_shader_variable_values(rage);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		rage->gui->draw(rage);
		glfwSwapBuffers(rage->window->glfw_window);

		clock_t endFrame = clock();

		double deltaTime = clockToMilliseconds(endFrame - beginFrame);
	}
	return (0);
}
