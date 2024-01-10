#include "RAGE.hpp"
#include "RAGE_primitive.hpp"
#include "RAGE_primitive_objects.hpp"
#include "physics/RAGE_ray_tracing.hpp"
#include "loaders/GLB_loader.hpp"

RAGE *get_rage()
{
	static RAGE *rage;
	if (rage == NULL)
		rage = new RAGE();
	return (rage);
}

int main(void)
{
	RAGE *rage;

	rage = get_rage();
	if (glfwInit() == GLFW_FALSE)
		return (1);

	std::filesystem::path executable_path = getExecutableDir();
	rage->executable_path = executable_path.generic_string();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	if (rage->window->init() == false)
		return (1);
	glm::ivec2 pixel_size = rage->window->pixel_size;
	if (rage->camera.init(rage->window) == false)
		return (1);

	glfwMakeContextCurrent(rage->window->glfw_window);
	gladLoadGL();

	set_callbacks(rage->window->glfw_window, rage);
	rage->gui = new RAGE_gui(rage);
	rage->shader = new RAGE_shader(rage->executable_path + "/shaders/vertex_test.glsl",
								   rage->executable_path + "/shaders/fragment_test.glsl");
	rage->shader->init_variable_locations();
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GLB_loader glb_loader;
	glb_loader.load((rage->executable_path + "/assets/models/SingleChildCubeIcosphere.glb").c_str());
	//glb_loader.load((rage->executable_path + "/assets/models/ParentingTest.glb").c_str());	

	while (glfwWindowShouldClose(rage->window->glfw_window) == GLFW_FALSE)
	{
		std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(rage->shader->hProgram);
		set_shader_variable_values(rage);

		rage->gui->draw(rage);
		glfwSwapBuffers(rage->window->glfw_window);

		if (rage->gui->scene_view.is_focused())
			rage->camera.handle_input(rage->user_input, (float)rage->delta_time);

		glfwPollEvents();
		glfwSwapInterval(0);

		std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end - start;
		rage->delta_time = elapsed.count();
	}
	return (0);
}