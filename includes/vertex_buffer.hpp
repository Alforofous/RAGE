#pragma once

#include "glad/glad.h"

class vertex_buffer
{
public:
	GLuint	id;
	vertex_buffer(GLfloat *vertices, GLsizeiptr size);
	
	void	bind();
	void	unbind();
	void	delete_object();
};