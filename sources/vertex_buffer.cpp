#include "vertex_buffer.hpp"

vertex_buffer::vertex_buffer(void *vertices, GLsizeiptr size)
{
	glGenBuffers(1, &this->id);
	glBindBuffer(GL_ARRAY_BUFFER, this->id);
	glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
}

void vertex_buffer::bind()
{
	glBindBuffer(GL_ARRAY_BUFFER, this->id);
}

void vertex_buffer::unbind()
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void vertex_buffer::delete_object()
{
	glDeleteBuffers(1, &this->id);
}
