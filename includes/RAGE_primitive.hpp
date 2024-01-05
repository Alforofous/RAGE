#pragma once

#include "vertex_array.hpp"
#include "buffer_object.hpp"
#include "buffer_object.hpp"
#include <vector>

class RAGE_primitive
{
public:
	RAGE_primitive();
	~RAGE_primitive();
	bool init(GLfloat *vertices, GLuint *indices, GLsizeiptr vertices_size, GLsizeiptr indices_size);
	bool interleave_vbos();
	void draw();
	bool is_initialized();

	vertex_array *vertex_array_object;
	buffer_object *interleaved_vertex_buffer_object;
	buffer_object *element_buffer_object;
	std::map<std::string, buffer_object *> non_interleaved_vertex_buffer_objects;
private:
	GLuint indices_count;
};