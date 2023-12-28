#pragma once

#include "glad/glad.h"

class object_buffer
{
public:
	GLuint id;
	object_buffer(GLenum type, void *data, GLsizeiptr byte_size);

	void bind();
	void unbind();
	void delete_object();
	GLenum get_type() { return (this->type); }
private:
	GLenum type;
};