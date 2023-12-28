#include "object_buffer.hpp"

object_buffer::object_buffer(GLenum type, void *data, GLsizeiptr byte_size)
{
	this->type = type;
	glGenBuffers(1, &this->id);
	glBindBuffer(this->type, this->id);
	glBufferData(this->type, byte_size, data, GL_STATIC_DRAW);
}

void object_buffer::bind()
{
	glBindBuffer(this->type, this->id);
}

void object_buffer::unbind()
{
	glBindBuffer(this->type, 0);
}

void object_buffer::delete_object()
{
	glDeleteBuffers(1, &this->id);
}