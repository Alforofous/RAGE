#include "RAGE.hpp"

int main(void)
{
	RAGE *rage;

	rage = new RAGE();
	if (glfwInit() == GLFW_FALSE)
		return (1);

	std::filesystem::path executable_path = getExecutableDir();
	rage->executable_path = executable_path.string();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	if (rage->window->Init() == -1)
		return (1);

	glfwMakeContextCurrent(rage->window->glfw_window);
	gladLoadGL();

	// Vertices coordinates
	GLfloat vertices[] =
		{
			-0.5f, -0.5f * float(sqrt(3)) / 3, 0.0f,	// Lower left corner
			0.5f, -0.5f * float(sqrt(3)) / 3, 0.0f,		// Lower right corner
			0.0f, 0.5f * float(sqrt(3)) * 2 / 3, 0.0f,	// Upper corner
			-0.5f / 2, 0.5f * float(sqrt(3)) / 6, 0.0f, // Inner left
			0.5f / 2, 0.5f * float(sqrt(3)) / 6, 0.0f,	// Inner right
			0.0f, -0.5f * float(sqrt(3)) / 3, 0.0f		// Inner down
		};

	// Indices for vertices order
	GLuint indices[] =
		{
			0, 3, 5, // Lower left triangle
			3, 2, 4, // Upper triangle
			5, 4, 1	 // Lower right triangle
		};

	set_callbacks(rage);
	rage->gui = new RAGE_gui(rage);

	rage->shader = new RAGE_shader(rage->executable_path + "/shaders/vertex_test.glsl",
								   rage->executable_path + "/shaders/fragment_test.glsl");

	/*Set GLSL variable locations*/
	rage->shader->InitVariableLocations();

	rage->init_gl_objects(vertices, indices, sizeof(vertices), sizeof(indices));

	glm::vec3 position(0.0, 0.0, -3.0);
	rage->camera->SetPosition(position);
	while (!glfwWindowShouldClose(rage->window->glfw_window))
	{
		std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

		int width;
		int height;
		glfwGetWindowSize(rage->window->glfw_window, &width, &height);
		glViewport(0, 0, width, height);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(rage->shader->hProgram);
		rage->vertex_array_object->bind();
		set_shader_variable_values(rage);
		glDrawElements(GL_TRIANGLES, 9, GL_UNSIGNED_INT, 0);
		rage->gui->draw(rage);
		glfwSwapBuffers(rage->window->glfw_window);

		glfwPollEvents();
		glfwSwapInterval(1);

		std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end - start;
		rage->delta_time = elapsed.count();
	}
	return (0);
}
