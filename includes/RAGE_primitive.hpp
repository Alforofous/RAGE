#pragma once

#include "loaders/GLB_attribute_buffer.hpp"

#include "vertex_array.hpp"
#include "buffer_object.hpp"
#include "buffer_object.hpp"
#include <vector>

class RAGE_primitive
{
public:
	RAGE_primitive();
	~RAGE_primitive();
	bool interleave_vbos();
	void draw();
	bool is_initialized();

	vertex_array *vertex_array_object;
	buffer_object *interleaved_vertex_buffer_object;
	buffer_object *element_buffer_object;
	std::vector<GLB_attribute_buffer *> attribute_buffers;
private:
	GLuint indices_count;
};