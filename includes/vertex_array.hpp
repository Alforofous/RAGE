#pragma once
#include <glad/glad.h>
#include "buffer_object.hpp"
#include <map>
#include <string>

struct GL_attribute
{
	buffer_object *vertex_buffer_object;
	GLuint component_count;
	GLenum data_type;
	GLsizei stride;
	void *offset;
};

class vertex_array
{
public:
	vertex_array();
	~vertex_array();

	void link_attributes(buffer_object &vertex_buffer_object, GLuint layout, GLuint component_count, GLenum type, GLsizei stride, void *offset);
	void bind();
	void unbind();
	std::string get_linked_attribute_data(GLuint layout);
private:
	GLuint id;
	std::map<GLuint, GL_attribute> attributes;
};