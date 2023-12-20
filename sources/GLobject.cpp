#include "GLobject.hpp"
#include "RAGE_mesh.hpp"
#include <iostream>

GLobject::GLobject()
{
	this->initialized = false;
}

bool GLobject::init(GLfloat *vertices, GLuint *indices, GLsizeiptr vertices_size, GLsizeiptr indices_size)
{
	try
	{
		this->vertex_array_object = new vertex_array();
		this->vertex_array_object->bind();

		this->vertex_buffer_object = new vertex_buffer(vertices, vertices_size);
		this->element_buffer_object = new element_buffer(indices, indices_size);

		// position
		this->vertex_array_object->link_attributes(*this->vertex_buffer_object, 0, 3, GL_FLOAT, 7 * sizeof(GLfloat), (void *)0);
		// color
		this->vertex_array_object->link_attributes(*this->vertex_buffer_object, 1, 4, GL_FLOAT, 7 * sizeof(GLfloat), (void *)(3 * sizeof(GLfloat)));
		vertex_array_object->unbind();
		this->vertex_buffer_object->unbind();
		this->element_buffer_object->unbind();

		this->indices_count = indices_size / sizeof(GLuint);
		this->initialized = true;
		return (true);
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error initializing GL object: " << e.what() << std::endl;
		return (false);
	}
}

bool GLobject::init(RAGE_mesh &mesh)
{
	bool gl_object_initialized = this->init(mesh.vertices, mesh.indices, mesh.vertices_size, mesh.indices_size);
	return (gl_object_initialized);
}

void GLobject::draw()
{
	this->vertex_array_object->bind();
	this->element_buffer_object->bind();
	glDrawElements(GL_TRIANGLES, this->indices_count, GL_UNSIGNED_INT, 0);
	this->element_buffer_object->unbind();
	this->vertex_array_object->unbind();
}

bool GLobject::is_initialized()
{
	return (this->initialized);
}

GLobject::~GLobject()
{
	this->vertex_array_object->delete_object();
	this->vertex_buffer_object->delete_object();
	this->element_buffer_object->delete_object();
	this->initialized = false;
}
