#pragma once
#include <glad/glad.h>
#include "buffer_object.hpp"
#include <map>

class vertex_array
{
public:
	vertex_array();

	void	link_attributes(buffer_object &buffer_object, GLuint layout, GLuint component_count, GLenum type, GLsizei stride, void *offset);
	void	bind();
	void	unbind();
	void	delete_object();
	std::string get_linked_attribute_data(GLuint layout);
private:
	GLuint	id;
	std::map<GLuint, GL_attribute> attributes;
};

struct GL_attribute
{
	buffer_object buffer_object;
	GLuint component_count;
	GLenum type;
	GLsizei stride;
	void *offset;
};
