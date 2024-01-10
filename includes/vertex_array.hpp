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
	std::string name;
};

class vertex_array
{
public:
	vertex_array();
	~vertex_array();

	void link_attributes(buffer_object *vertex_buffer_object, GLuint layout, GLuint component_count, GLenum gl_data_type, GLsizei stride, void *offset, std::string name);
	void bind();
	void unbind();
	std::string get_linked_attribute_data(GLuint layout);
	std::string get_linked_attributes_data();
	const std::map<GLuint, GL_attribute> *get_linked_attributes() const;
	const GL_attribute *get_linked_attribute(GLuint layout);

private:
	GLuint id;
	std::map<GLuint, GL_attribute> linked_attributes;
};