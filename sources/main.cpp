#include "RAGE.hpp"
#include "RAGE_primitive.hpp"
#include "RAGE_primitive_objects.hpp"
#include "physics/RAGE_ray_tracing.hpp"
#include "loaders/GLB_loader.hpp"
#include "RAGE_material_skybox.hpp"

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
	if (rage->camera.init(rage->window) == false)
		return (1);

	glfwMakeContextCurrent(rage->window->glfw_window);
	gladLoadGL();

	set_callbacks(rage->window->glfw_window, rage);
	rage->gui = new RAGE_gui(rage);
	RAGE_shader::init_RAGE_shaders(rage);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GLB_loader glb_loader;
	//rage->scene = glb_loader.load((rage->executable_path + "/assets/models/SingleChildCubeIcosphere.glb").c_str());
	//rage->scene = glb_loader.load((rage->executable_path + "/assets/models/CubeVertexColored.glb").c_str());
	//rage->scene = glb_loader.load((rage->executable_path + "/assets/models/BoxVertexColors.glb").c_str());
	rage->scene = glb_loader.load((rage->executable_path + "/assets/models/Duck.glb").c_str());

	RAGE_mesh *mesh = new RAGE_mesh(new RAGE_material());
	RAGE_object *object = new RAGE_object(mesh, "Cube");
	object->mesh->primitives.push_back(RAGE_primitive_objects::create_cube());
	rage->scene->add_object(object);

	RAGE_mesh *mesh2 = new RAGE_mesh(new RAGE_material_skybox());
	RAGE_object *object2 = new RAGE_object(mesh2, "Cube2");
	object2->mesh->primitives.push_back(RAGE_primitive_objects::create_cube());
	rage->scene->add_object(object2);

	if (rage->scene == NULL)
		return (1);
	glfwSwapInterval(rage->window->vsync);
	while (glfwWindowShouldClose(rage->window->glfw_window) == GLFW_FALSE)
	{
		std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

		rage->shader->update_uniform("u_view_matrix", rage);
		rage->shader->update_uniform("u_perspective_matrix", rage);
		rage->gui->draw(rage);
		if (rage->gui->scene_view.is_focused())
			rage->camera.handle_input(rage->user_input, (float)rage->delta_time);

		glfwSwapBuffers(rage->window->glfw_window);
		glfwPollEvents();
		std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end - start;
		rage->delta_time = elapsed.count();
	}
	return (0);
}