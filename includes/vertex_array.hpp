#pragma once
#include <glad/glad.h>
#include "object_buffer.hpp"

class vertex_array
{
public:
	GLuint	id;
	vertex_array();

	void	link_attributes(object_buffer &object_buffer, GLuint layout, GLuint num_components, GLenum type, GLsizei stride, void *offset);
	void	bind();
	void	unbind();
	void	delete_object();
};