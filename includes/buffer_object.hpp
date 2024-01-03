#pragma once

#include "glad/glad.h"

class buffer_object
{
public:
	GLuint id;
	buffer_object(GLenum type, void *data, GLsizeiptr byte_size);

	void bind();
	void unbind();
	void delete_object();
	GLenum get_type() { return (this->type); }
private:
	GLenum type;
};