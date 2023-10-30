#pragma once
#include <glad/glad.h>
#include "vertex_array_buffer.hpp"

class vertex_array
{
public:
	GLuint	id;
	vertex_array();

	void	link_to_vertex_array_buffer(vertex_array_buffer &vertex_array_buffer, GLuint layout);
	void	bind();
	void	unbind();
	void	delete_object();
};