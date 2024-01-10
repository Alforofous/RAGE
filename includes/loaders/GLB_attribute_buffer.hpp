#pragma once

#include "glad/glad.h"
#include <string>

class GLB_attribute_buffer
{
public:
	GLB_attribute_buffer(GLvoid *glb_buffer, GLsizeiptr byte_offset, GLsizeiptr count, std::string name, GLsizeiptr component_count, GLenum gl_data_type, GLboolean normalized, GLsizei stride);
	~GLB_attribute_buffer();

	void print_data();
private:
	std::string name;
	GLsizeiptr component_count;
	GLenum gl_data_type;
	GLboolean normalized;
	GLsizei stride;

	GLvoid *data;
	GLsizeiptr size;
};