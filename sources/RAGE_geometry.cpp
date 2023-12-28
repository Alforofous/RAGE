#include "RAGE_geometry.hpp"
#include <iostream>

RAGE_geometry::RAGE_geometry()
{
	this->initialized = false;
}

bool RAGE_geometry::init(GLfloat *vertices, GLuint *indices, GLsizeiptr vertices_size, GLsizeiptr indices_size)
{
	try
	{
		this->vertex_array_object = new vertex_array();
		this->vertex_array_object->bind();

		this->vertex_buffer_object = new object_buffer(GL_ARRAY_BUFFER, vertices, vertices_size);
		this->element_buffer_object = new object_buffer(GL_ELEMENT_ARRAY_BUFFER, indices, indices_size);

		this->vertex_array_object->link_attributes(*this->vertex_buffer_object, 0, 3, GL_FLOAT, 7 * sizeof(GLfloat), (void *)0);
		this->vertex_array_object->link_attributes(*this->element_buffer_object, 1, 4, GL_FLOAT, 7 * sizeof(GLfloat), (void *)(3 * sizeof(GLfloat)));
		this->vertex_array_object->unbind();
		this->vertex_buffer_object->unbind();
		this->element_buffer_object->unbind();

		this->indices_count = (GLuint)(indices_size / sizeof(GLuint));
		this->initialized = true;
		return (true);
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error initializing GL object: " << e.what() << std::endl;
		return (false);
	}
}

void RAGE_geometry::draw()
{
	this->vertex_array_object->bind();
	this->element_buffer_object->bind();
	glDrawElements(GL_TRIANGLES, this->indices_count, GL_UNSIGNED_INT, 0);
	this->element_buffer_object->unbind();
	this->vertex_array_object->unbind();
}

bool RAGE_geometry::is_initialized()
{
	return (this->initialized);
}

RAGE_geometry::~RAGE_geometry()
{
	this->vertex_array_object->delete_object();
	this->vertex_buffer_object->delete_object();
	this->element_buffer_object->delete_object();
	this->initialized = false;
}
