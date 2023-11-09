#include "vertex_buffer.hpp"

vertex_buffer::vertex_buffer(GLfloat *vertices, GLsizeiptr size)
{
	glGenBuffers(1, &id);
	glBindBuffer(GL_ARRAY_BUFFER, id);
	glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
}

void	vertex_buffer::bind()
{
	glBindBuffer(GL_ARRAY_BUFFER, id);
}

void	vertex_buffer::unbind()
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void	vertex_buffer::delete_object()
{
	glDeleteBuffers(1, &id);
}
