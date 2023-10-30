#include "vertex_array.hpp"

vertex_array::vertex_array()
{
	glGenVertexArrays(1, &id);
}

void vertex_array::link_to_vertex_array_buffer(vertex_array_buffer &vertex_array_buffer, GLuint layout)
{
	vertex_array_buffer.bind();
	glVertexAttribPointer(layout, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
	glEnableVertexAttribArray(layout);
	vertex_array_buffer.unbind();
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
