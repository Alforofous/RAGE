#pragma once

#include "glad/glad.h"
#include <vector>

class buffer_object
{
public:
	buffer_object(GLenum buffer_type, GLenum data_type, void *data, GLsizeiptr byte_size);
	buffer_object();
	~buffer_object();

	void *get_data();
	GLuint get_byte_size();
	size_t get_vertex_count();
	void bind();
	void unbind();
	void push_back(void *data, GLsizeiptr byte_size);
	void push_empty(GLsizeiptr byte_size);
	void update_data(void *data, GLsizeiptr byte_size);
	void print_data(int stride_size);
	static buffer_object *create_from_glb_buffer(GLenum gl_buffer_type, std::vector<char> &glb_buffer, size_t byte_offset, size_t count, GLenum data_type);
private:
	void init(GLenum buffer_type, GLenum data_type, void *data, GLsizeiptr byte_size);

	GLuint id;
	void *data;
	GLuint byte_size;
	GLenum data_type;
	GLenum buffer_type;
	size_t vertex_count;
};