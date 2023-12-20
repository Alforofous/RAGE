#include "RAGE.hpp"
#include "GLobject.hpp"
#include "RAGE_primitive_objects.hpp"

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

	if (rage->window->init() == false)
		return (1);
	glm::ivec2 window_pixel_size = rage->window->get_window_pixel_size();
	if (rage->camera.init(rage->window) == false)
		return (1);

	glfwMakeContextCurrent(rage->window->glfw_window);
	gladLoadGL();

	RAGE_mesh mesh;
	RAGE_mesh mesh2;
	mesh.LoadGLB((rage->executable_path + "/assets/models/SimpleCone.glb").c_str());
	mesh.LoadGLB((rage->executable_path + "/assets/models/MonkeyHead.glb").c_str());
	mesh.LoadGLB((rage->executable_path + "/assets/models/Duck.glb").c_str());

	mesh.LoadGLB((rage->executable_path + "/assets/models/CubeVertexColored.glb").c_str());
	mesh.LoadGLB((rage->executable_path + "/assets/models/WideMonkeyHeadVertexColored.glb").c_str());
	mesh2.LoadGLB((rage->executable_path + "/assets/models/BoxVertexColors.glb").c_str());

	rage->scene.add_object(new RAGE_object(&mesh2, "BoxVertexColors"));
	rage->scene.add_object(new RAGE_object(&mesh, "WideMonkeyHeadVertexColored"));
	rage->scene.add_object(RAGE_primitive_objects::create_cube(20.0f, 20.0f, 0.1f));

	set_callbacks(rage);
	rage->gui = new RAGE_gui(rage);
	rage->shader = new RAGE_shader(rage->executable_path + "/shaders/vertex_test.glsl",
								   rage->executable_path + "/shaders/fragment_test.glsl");
	rage->shader->InitVariableLocations();
	rage->scene.print_objects();
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	while (glfwWindowShouldClose(rage->window->glfw_window) == GLFW_FALSE)
	{
		std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

		int width;
		int height;
		glfwGetFramebufferSize(rage->window->glfw_window, &width, &height);
		glViewport(0, 0, width, height);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(rage->shader->hProgram);
		set_shader_variable_values(rage);

		rage->scene.draw();

		rage->gui->draw(rage);
		glfwSwapBuffers(rage->window->glfw_window);
		rage->camera.handle_input(rage->user_input, (float)rage->delta_time);

		glfwPollEvents();
		glfwSwapInterval(0);

		std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end - start;
		rage->delta_time = elapsed.count();
	}
	return (0);
}