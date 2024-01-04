#pragma once

#include "glad/glad.h"
#include <vector>

class buffer_object
{
public:
	GLuint id;
	buffer_object(GLenum type, void *data, GLsizeiptr byte_size);

	void *get_data();
	GLuint get_byte_size();
	void bind();
	void unbind();
	void delete_object();
	template <typename buffer_data_type>
	static buffer_object *create_from_glb_buffer(GLenum gl_buffer_type, std::vector<char> &glb_buffer, size_t byte_offset, size_t count)
	{
		buffer_data_type *gl_buffer_data;
		buffer_object *gl_buffer_object;

		gl_buffer_data = reinterpret_cast<buffer_data_type *>(&glb_buffer[byte_offset]);
		gl_buffer_object = new buffer_object(gl_buffer_type, (void *)gl_buffer_data, count * sizeof(buffer_data_type));
		return (gl_buffer_object);
	};
private:
	void *data;
	GLuint byte_size;
	GLenum type;
};