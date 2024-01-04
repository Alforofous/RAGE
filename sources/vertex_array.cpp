#include "vertex_array.hpp"
#include <string>

vertex_array::vertex_array()
{
	glGenVertexArrays(1, &id);
}

void vertex_array::link_attributes(buffer_object &buffer_object, GLuint layout, GLuint component_count, GLenum type, GLsizei stride, void *offset)
{
	attributes[layout] = {buffer_object, component_count, type, stride, offset};

	buffer_object.bind();
	glVertexAttribPointer(layout, component_count, type, GL_FALSE, stride, offset);
	glEnableVertexAttribArray(layout);
	buffer_object.unbind();
}

template <typename T>
static std::string get_attribute_data_with_type(void *data, size_t size, GLuint component_count)
{
	T *typed_data = reinterpret_cast<T *>(data);
	std::string result;
	for (size_t i = 0; i < size / sizeof(T); i++)
	{
		result += std::to_string(typed_data[i]);
		if ((i + 1) % component_count == 0)
			result += "\n";
		else
			result += " ";
	}
	return (result);
}

std::string vertex_array::get_linked_attribute_data(GLuint layout)
{
	if (attributes.find(layout) == attributes.end())
		return ("No attribute found for layout " + std::to_string(layout) + ".");
	GL_attribute attribute = attributes[layout];
	std::string result = "Attribute for layout " + std::to_string(layout) + ":\n";
	void *data = attribute.buffer_object.get_data();
	GLuint byte_size = attribute.buffer_object.get_byte_size();
	if (attribute.type == GL_BYTE)
		result += get_attribute_data_with_type<GLbyte>(data, byte_size, attribute.component_count);
	else if (attribute.type == GL_UNSIGNED_BYTE)
		result += get_attribute_data_with_type<GLubyte>(data, byte_size, attribute.component_count);
	else if (attribute.type == GL_SHORT)
		result += get_attribute_data_with_type<GLshort>(data, byte_size, attribute.component_count);
	else if (attribute.type == GL_UNSIGNED_SHORT)
		result += get_attribute_data_with_type<GLushort>(data, byte_size, attribute.component_count);
	else if (attribute.type == GL_INT)
		result += get_attribute_data_with_type<GLint>(data, byte_size, attribute.component_count);
	else if (attribute.type == GL_UNSIGNED_INT)
		result += get_attribute_data_with_type<GLuint>(data, byte_size, attribute.component_count);
	else if (attribute.type == GL_FLOAT)
		result += get_attribute_data_with_type<GLfloat>(data, byte_size, attribute.component_count);
	else if (attribute.type == GL_DOUBLE)
		result += get_attribute_data_with_type<GLdouble>(data, byte_size, attribute.component_count);
	else
		result += "Unknown type.";
	return result;
}

void	vertex_array::bind()
{
	glBindVertexArray(id);
}

void	vertex_array::unbind()
{
	glBindVertexArray(0);
}

void	vertex_array::delete_object()
{
	glDeleteVertexArrays(1, &id);
}
