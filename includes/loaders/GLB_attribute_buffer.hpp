#pragma once

#include "glad/glad.h"
#include <string>

class GLB_attribute_buffer
{
public:
	GLB_attribute_buffer(GLvoid *glb_buffer, GLsizeiptr byte_offset, GLsizeiptr vertex_count, std::string name, GLsizeiptr component_count, GLenum gl_data_type);
	~GLB_attribute_buffer();

	std::string get_data_string();
	std::string get_name();
	GLsizeiptr get_component_count();
	GLsizeiptr get_vertex_count();
	GLsizeiptr get_element_count();
	GLenum get_gl_data_type();
	GLvoid *get_data();
	GLsizeiptr get_byte_size();
private:
	std::string name;
	GLenum gl_data_type;
	GLsizeiptr component_count;
	GLsizeiptr vertex_count;

	GLvoid *data;
	GLsizeiptr byte_size;
};