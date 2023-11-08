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

void RAGE::init_gl_objects(GLfloat *vertices, GLuint *indices, GLsizeiptr vertice_size, GLsizeiptr indices_size)
{
	vertex_array_object = new vertex_array();
	vertex_array_object->bind();

	vertex_array_buffer_object = new vertex_array_buffer(vertices, vertice_size);
	element_array_buffer_object = new element_array_buffer(indices, indices_size);
	
	vertex_array_object->link_attributes(*vertex_array_buffer_object, 0, 3, GL_FLOAT, 6 * sizeof(GLfloat), (void *)0);
	vertex_array_object->link_attributes(*vertex_array_buffer_object, 1, 3, GL_FLOAT, 6 * sizeof(GLfloat), (void *)(3 * sizeof(GLfloat)));
	vertex_array_object->unbind();
	vertex_array_buffer_object->unbind();
	element_array_buffer_object->unbind();
}
