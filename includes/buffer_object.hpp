#pragma once

#include "glad/glad.h"
#include <vector>
#include <string>

class buffer_object
{
public:
	buffer_object(GLenum buffer_type, GLenum data_type, void *data, GLsizeiptr byte_size);
	buffer_object();
	~buffer_object();

	void *get_data();
	GLsizeiptr get_byte_size();
	size_t get_vertex_count();
	GLenum get_data_type();
	void bind();
	void unbind();
	static buffer_object *create_ebo_from_glb_buffer(GLenum gl_buffer_type, std::vector<char> &glb_buffer, size_t byte_offset, size_t count, GLenum data_type);
	std::string get_data_string(int component_count);
	std::string get_data_string(int component_count, GLenum data_type);
	void update_gl_buffer();
	std::vector<GLubyte> data;

private:
	void init(GLenum buffer_type, GLenum data_type, void *data, GLsizeiptr byte_size);

	GLuint id;
	GLsizeiptr byte_size;
	GLenum data_type;
	GLenum buffer_type;
	size_t vertex_count;
};