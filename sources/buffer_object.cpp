#include "buffer_object.hpp"

buffer_object::buffer_object(GLenum type, void *data, GLsizeiptr byte_size)
{
	this->type = type;
	this->data = data;
	this->byte_size = byte_size;
	glGenBuffers(1, &this->id);
	glBindBuffer(this->type, this->id);
	glBufferData(this->type, byte_size, data, GL_STATIC_DRAW);
}

void *buffer_object::get_data()
{
	return (this->data);
}

GLuint buffer_object::get_byte_size()
{
	return (this->byte_size);
}

void buffer_object::bind()
{
	glBindBuffer(this->type, this->id);
}

void buffer_object::unbind()
{
	glBindBuffer(this->type, 0);
}

void buffer_object::delete_object()
{
	glDeleteBuffers(1, &this->id);
}