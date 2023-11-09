#include "element_buffer.hpp"

element_buffer::element_buffer(GLuint *indices, GLsizeiptr size)
{
	glGenBuffers(1, &id);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, GL_STATIC_DRAW);
}

void	element_buffer::bind()
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
}

void	element_buffer::unbind()
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void	element_buffer::delete_object()
{
	glDeleteBuffers(1, &id);
}

