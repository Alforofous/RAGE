#pragma once
#include <glad/glad.h>
#include "buffer_object.hpp"

class vertex_array
{
public:
	GLuint	id;
	vertex_array();

	void	link_attributes(buffer_object &buffer_object, GLuint layout, GLuint num_components, GLenum type, GLsizei stride, void *offset);
	void	bind();
	void	unbind();
	void	delete_object();
};