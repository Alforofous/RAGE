#pragma once
#include <glad/glad.h>
#include "vertex_buffer.hpp"

class vertex_array
{
public:
	GLuint	id;
	vertex_array();

	void	link_attributes(vertex_buffer &vertex_buffer, GLuint layout, GLuint num_components, GLenum type, GLsizei stride, void *offset);
	void	bind();
	void	unbind();
	void	delete_object();
};