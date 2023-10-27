#include "RAGE.hpp"

int main(void)
{
	RAGE	*rage;

	rage = new RAGE();
	if (glfwInit() == GLFW_FALSE)
		return (1);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	if (rage->window->Init() == -1)
		return (1);

	glfwMakeContextCurrent(rage->window->glfw_window);
	gladLoadGL();

	GLfloat vertices[] =
	{
		-0.5f, -0.5f * float(sqrt(3)) / 3, 0.0f, // Lower left corner
		0.5f, -0.5f * float(sqrt(3)) / 3, 0.0f, // Lower right corner
		0.0f, 0.5f * float(sqrt(3)) * 2 / 3, 0.0f // Upper corner
	};

	set_callbacks(rage);
	rage->gui = new RAGE_gui(rage->window->glfw_window);

	/*Set GLSL variable locations*/
	rage->shader = new RAGE_shader("../shaders/vertex_test.glsl", "../shaders/fragment_test.glsl");

	rage->shader->InitVariableLocations();

	GLuint vertex_array_object;
	GLuint vertex_buffer_object;

	glGenVertexArrays(1, &vertex_array_object);
	glGenBuffers(1, &vertex_buffer_object);

	glBindVertexArray(vertex_array_object);

	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_object);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	rage->camera->SetPosition(glm::vec3(0.0, 0.0, -3.0));
	while (!glfwWindowShouldClose(rage->window->glfw_window))
	{
		clock_t beginFrame = clock();

		glfwPollEvents();
		int width;
		int height;
		glfwGetWindowSize(rage->window->glfw_window, &width, &height);
		glViewport(0, 0, width, height);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(rage->shader->hProgram);
		glBindVertexArray(vertex_array_object);
		set_shader_variable_values(rage);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		rage->gui->draw(rage);
		glfwSwapBuffers(rage->window->glfw_window);

		clock_t endFrame = clock();

		rage->delta_time = clockToMilliseconds(endFrame - beginFrame);
	}
	return (0);
}
