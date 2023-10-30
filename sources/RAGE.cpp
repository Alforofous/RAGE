#include "RAGE.hpp"

RAGE::RAGE()
{
	window = new RAGE_window();
	camera = new RAGE_camera();
}

RAGE::~RAGE()
{
	glfwDestroyWindow(window->glfw_window);
	glfwTerminate();
	vertex_array_object->delete_object();
	vertex_array_buffer_object->delete_object();
	element_array_buffer_object->delete_object();
}

void RAGE::init_gl_objects(GLfloat *vertices, GLuint *indices, GLsizeiptr vertice_size, GLsizeip)
{
	vertex_array_object = new vertex_array();
	vertex_array_object->bind();

	vertex_array_buffer_object = new vertex_array_buffer(vertices, sizeof(*vertices));
	element_array_buffer_object = new element_array_buffer(indices, sizeof(*indices));
	
	vertex_array_object->link_to_vertex_array_buffer(*vertex_array_buffer_object, 0);
	vertex_array_object->unbind();
	vertex_array_buffer_object->unbind();
	element_array_buffer_object->unbind();
}
