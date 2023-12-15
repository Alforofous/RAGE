#include "RAGE.hpp"

int main(void)
{
	RAGE *rage;

	rage = new RAGE();
	if (glfwInit() == GLFW_FALSE)
		return (1);

	std::filesystem::path executable_path = getExecutableDir();
	rage->executable_path = executable_path.generic_string();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	if (rage->window->Init() == -1)
		return (1);
	glm::ivec2 window_pixel_size = rage->window->get_window_pixel_size();
	if (rage->camera->Init(45.0f, window_pixel_size, glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f), glm::vec2(0.1f, 1000.0f)) == -1)
		return (1);

	glfwMakeContextCurrent(rage->window->glfw_window);
	gladLoadGL();

	GLfloat vertices[] = {
		0.0f, 5.0f, 0.0f, 1.0f, 0.0f, 0.0f,   // Peak
		-0.5f, 0.0f, -0.5f, 0.0f, 1.0f, 0.0f, // Base corner 1
		0.5f, 0.0f, -0.5f, 0.0f, 0.0f, 1.0f,  // Base corner 2
		0.5f, 0.0f, 0.5f, 1.0f, 1.0f, 0.0f,   // Base corner 3
		-0.5f, 0.0f, 0.5f, 0.0f, 1.0f, 1.0f   // Base corner 4
	};

	GLuint indices[] = {
		0, 1, 2, // Triangle 1
		0, 2, 3, // Triangle 2
		0, 3, 4, // Triangle 3
		0, 4, 1, // Triangle 4
		1, 2, 3, // Base triangle 1
		1, 3, 4	 // Base triangle 2
	};

	set_callbacks(rage);
	rage->gui = new RAGE_gui(rage);
	rage->shader = new RAGE_shader(rage->executable_path + "/shaders/vertex_test.glsl",
								   rage->executable_path + "/shaders/fragment_test.glsl");
	rage->shader->InitVariableLocations();
	rage->init_gl_objects(vertices, indices, sizeof(vertices), sizeof(indices));
	glEnable(GL_DEPTH_TEST);

	while (!glfwWindowShouldClose(rage->window->glfw_window))
	{
		std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

		int width;
		int height;
		glfwGetFramebufferSize(rage->window->glfw_window, &width, &height);
		glViewport(0, 0, width, height);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(rage->shader->hProgram);
		rage->vertex_array_object->bind();
		set_shader_variable_values(rage);
		int numIndices = sizeof(indices) / sizeof(indices[0]);
		glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
		rage->gui->draw(rage);
		glfwSwapBuffers(rage->window->glfw_window);
		rage->camera->handle_input(rage->user_input, (float)rage->delta_time);

		glfwPollEvents();
		glfwSwapInterval(0);

		std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end - start;
		rage->delta_time = elapsed.count();
	}
	return (0);
}