#include "vertex_array.hpp"

vertex_array::vertex_array()
{
	glGenVertexArrays(1, &id);
}

void vertex_array::link_attributes(vertex_buffer &vertex_buffer, GLuint layout, GLuint num_components, GLenum type, GLsizei stride, void *offset)
{
	vertex_buffer.bind();
	glVertexAttribPointer(layout, num_components, type, GL_FALSE, stride, offset);
	glEnableVertexAttribArray(layout);
	vertex_buffer.unbind();
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
