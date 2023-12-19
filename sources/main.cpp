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
	if (rage->camera->Init(rage->window) == -1)
		return (1);

	glfwMakeContextCurrent(rage->window->glfw_window);
	gladLoadGL();

	RAGE_mesh mesh;
	mesh.LoadGLB((rage->executable_path + "/assets/models/SimpleCone.glb").c_str());
	mesh.LoadGLB((rage->executable_path + "/assets/models/CubeVertexColored.glb").c_str());
	mesh.LoadGLB((rage->executable_path + "/assets/models/MonkeyHead.glb").c_str());
	GLfloat *vertices = mesh.vertices;
	GLuint *indices = mesh.indices;
	GLsizeiptr vertices_size = mesh.vertices_size;
	GLsizeiptr indices_size = mesh.indices_size;

	set_callbacks(rage);
	rage->gui = new RAGE_gui(rage);
	rage->shader = new RAGE_shader(rage->executable_path + "/shaders/vertex_test.glsl",
								   rage->executable_path + "/shaders/fragment_test.glsl");
	rage->shader->InitVariableLocations();
	rage->init_gl_objects(vertices, indices, vertices_size, indices_size);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
		int indices_count = indices_size / sizeof(indices[0]);
		glDrawElements(GL_TRIANGLES, indices_count, GL_UNSIGNED_INT, 0);
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