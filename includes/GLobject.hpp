#pragma once

#include "RAGE_mesh.hpp"
#include "vertex_array.hpp"
#include "vertex_buffer.hpp"
#include "element_buffer.hpp"

class GLobject
{
public:
	vertex_array *vertex_array_object;
	vertex_buffer *vertex_buffer_object;
	element_buffer *element_buffer_object;

	GLobject();
	~GLobject();
	bool init(GLfloat *vertices, GLuint *indices, GLsizeiptr vertices_size, GLsizeiptr indices_size);
	bool init(RAGE_mesh &mesh);
	void draw();
	bool is_initialized();
private:
	int indices_count;
	bool initialized;
};