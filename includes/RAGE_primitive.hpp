#pragma once

#include "vertex_array.hpp"
#include "buffer_object.hpp"
#include "buffer_object.hpp"

class RAGE_primitive
{
public:
	vertex_array *vertex_array_object;
	buffer_object *vertex_buffer_object;
	buffer_object *element_buffer_object;

	RAGE_primitive();
	~RAGE_primitive();
	bool init(GLfloat *vertices, GLuint *indices, GLsizeiptr vertices_size, GLsizeiptr indices_size);
	void draw();
	bool is_initialized();
private:
	GLuint indices_count;
	bool initialized;
};