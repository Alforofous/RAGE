#include "RAGE_primitive.hpp"
#include <iostream>

RAGE_primitive::RAGE_primitive()
{
	this->initialized = false;
	this->vertex_array_object = NULL;
	this->vertex_buffer_object = NULL;
	this->element_buffer_object = NULL;
}

bool RAGE_primitive::init(GLfloat *vertices, GLuint *indices, GLsizeiptr vertices_size, GLsizeiptr indices_size)
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
		std::cerr << "Error initializing primitive: " << e.what() << std::endl;
		return (false);
	}
}

void RAGE_primitive::draw()
{
	this->vertex_array_object->bind();
	this->element_buffer_object->bind();
	glDrawElements(GL_TRIANGLES, this->indices_count, GL_UNSIGNED_INT, 0);
	this->element_buffer_object->unbind();
	this->vertex_array_object->unbind();
}

bool RAGE_primitive::is_initialized()
{
	return (this->initialized);
}

RAGE_primitive::~RAGE_primitive()
{
	if (this->initialized == false)
		return;
	if (this->vertex_array_object != NULL)
		this->vertex_array_object->delete_object();
	if (this->vertex_buffer_object != NULL)
		this->vertex_buffer_object->delete_object();
	if (this->element_buffer_object != NULL)
		this->element_buffer_object->delete_object();
	this->vertex_array_object = NULL;
	this->vertex_buffer_object = NULL;
	this->element_buffer_object = NULL;
	this->initialized = false;
}
