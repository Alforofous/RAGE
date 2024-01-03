#pragma once

#include "vertex_array.hpp"
#include "object_buffer.hpp"
#include "object_buffer.hpp"

class RAGE_primitive
{
public:
	vertex_array *vertex_array_object;
	object_buffer *vertex_buffer_object;
	object_buffer *element_buffer_object;

	RAGE_primitive();
	~RAGE_primitive();
	bool init(GLfloat *vertices, GLuint *indices, GLsizeiptr vertices_size, GLsizeiptr indices_size);
	void draw();
	bool is_initialized();
private:
	GLuint indices_count;
	bool initialized;
};