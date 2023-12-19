#include "GLobject.hpp"

GLobject::~GLobject()
{
	this->vertex_array_object->delete_object();
	this->vertex_buffer_object->delete_object();
	this->element_buffer_object->delete_object();
}

void GLobject::init(GLfloat *vertices, GLuint *indices, GLsizeiptr vertices_size, GLsizeiptr indices_size)
{
	this->vertex_array_object = new vertex_array();
	this->vertex_array_object->bind();

	this->vertex_buffer_object = new vertex_buffer(vertices, vertices_size);
	this->element_buffer_object = new element_buffer(indices, indices_size);
	
	//position
	this->vertex_array_object->link_attributes(*this->vertex_buffer_object, 0, 3, GL_FLOAT, 7 * sizeof(GLfloat), (void *)0);
	//color
	this->vertex_array_object->link_attributes(*this->vertex_buffer_object, 1, 4, GL_FLOAT, 7 * sizeof(GLfloat), (void *)(3 * sizeof(GLfloat)));vertex_array_object->unbind();
	this->vertex_buffer_object->unbind();
	this->element_buffer_object->unbind();

	this->indices_count = indices_size / sizeof(GLuint);
}

void GLobject::draw()
{
	this->vertex_array_object->bind();
	this->element_buffer_object->bind();
	glDrawElements(GL_TRIANGLES, this->indices_count, GL_UNSIGNED_INT, 0);
	this->element_buffer_object->unbind();
	this->vertex_array_object->unbind();
}