#include "vertex_array.hpp"

vertex_array::vertex_array()
{
	glGenVertexArrays(1, &id);
}

void vertex_array::link_attributes(buffer_object &buffer_object, GLuint layout, GLuint num_components, GLenum type, GLsizei stride, void *offset)
{
	buffer_object.bind();
	glVertexAttribPointer(layout, num_components, type, GL_FALSE, stride, offset);
	glEnableVertexAttribArray(layout);
	buffer_object.unbind();
}

void	vertex_array::bind()
{
	glBindVertexArray(id);
}

void	vertex_array::unbind()
{
	glBindVertexArray(0);
}

void	vertex_array::delete_object()
{
	glDeleteVertexArrays(1, &id);
}
