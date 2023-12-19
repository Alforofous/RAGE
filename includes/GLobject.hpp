#pragma once

#include "vertex_array.hpp"
#include "vertex_buffer.hpp"
#include "element_buffer.hpp"

class GLobject
{
public:
	vertex_array *vertex_array_object;
	vertex_buffer *vertex_buffer_object;
	element_buffer *element_buffer_object;

	~GLobject();
	void init(GLfloat *vertices, GLuint *indices, GLsizeiptr vertices_size, GLsizeiptr indices_size);
	void draw();

private:
	int indices_count;
};