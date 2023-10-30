#include "vertex_array_buffer.hpp"

vertex_array_buffer::vertex_array_buffer(GLfloat *vertices, GLsizeiptr size)
{
	glGenBuffers(1, &id);
	glBindBuffer(GL_ARRAY_BUFFER, id);
	glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
}

void	vertex_array_buffer::bind()
{
	glBindBuffer(GL_ARRAY_BUFFER, id);
}

void	vertex_array_buffer::unbind()
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void	vertex_array_buffer::delete_object()
{
	glDeleteBuffers(1, &id);
}
