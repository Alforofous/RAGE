#include "vertex_array.hpp"
#include <string>

vertex_array::vertex_array()
{
	glGenVertexArrays(1, &id);
}

void vertex_array::link_attributes(buffer_object *vertex_buffer_object, GLuint layout, GLuint component_count, GLenum gl_data_type, GLsizei stride, void *offset, std::string name)
{
	linked_attributes[layout] = {vertex_buffer_object, component_count, gl_data_type, stride, offset, name};

	vertex_buffer_object->bind();
	glVertexAttribPointer(layout, component_count, gl_data_type, GL_FALSE, stride, offset);
	glEnableVertexAttribArray(layout);
	vertex_buffer_object->unbind();
}

template <typename T>
static std::string get_attribute_data_with_data_type(void *data, size_t size, GLuint component_count)
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
	if (linked_attributes.find(layout) == linked_attributes.end())
		return ("No attribute found for layout " + std::to_string(layout) + ".");
	GL_attribute linked_attribute = linked_attributes[layout];
	std::string result = linked_attribute.name + " ATTRIBUTE:\n";
	void *data = linked_attribute.vertex_buffer_object->get_data();
	if (data == NULL)
		return ("Data is NULL.");
	GLuint byte_size = linked_attribute.vertex_buffer_object->get_byte_size();
	if (linked_attribute.data_type == GL_BYTE)
		result += get_attribute_data_with_data_type<GLbyte>(data, byte_size, linked_attribute.component_count);
	else if (linked_attribute.data_type == GL_UNSIGNED_BYTE)
		result += get_attribute_data_with_data_type<GLubyte>(data, byte_size, linked_attribute.component_count);
	else if (linked_attribute.data_type == GL_SHORT)
		result += get_attribute_data_with_data_type<GLshort>(data, byte_size, linked_attribute.component_count);
	else if (linked_attribute.data_type == GL_UNSIGNED_SHORT)
		result += get_attribute_data_with_data_type<GLushort>(data, byte_size, linked_attribute.component_count);
	else if (linked_attribute.data_type == GL_INT)
		result += get_attribute_data_with_data_type<GLint>(data, byte_size, linked_attribute.component_count);
	else if (linked_attribute.data_type == GL_UNSIGNED_INT)
		result += get_attribute_data_with_data_type<GLuint>(data, byte_size, linked_attribute.component_count);
	else if (linked_attribute.data_type == GL_FLOAT)
		result += get_attribute_data_with_data_type<GLfloat>(data, byte_size, linked_attribute.component_count);
	else if (linked_attribute.data_type == GL_DOUBLE)
		result += get_attribute_data_with_data_type<GLdouble>(data, byte_size, linked_attribute.component_count);
	else
		result += "Unknown type.";
	return result;
}

std::string vertex_array::get_linked_attributes_data()
{
	std::string result = "Linked attributes:\n";
	for (auto &attribute : linked_attributes)
	{
		result += get_linked_attribute_data(attribute.first);
		result += "\n";
	}
	return (result);
}

const std::map<GLuint, GL_attribute> *vertex_array::get_linked_attributes() const
{
	return (&linked_attributes);
}

const GL_attribute *vertex_array::get_linked_attribute(GLuint layout)
{
	if (linked_attributes.find(layout) == linked_attributes.end())
		return (NULL);
	return (&linked_attributes[layout]);
}

void vertex_array::bind()
{
	glBindVertexArray(id);
}

void vertex_array::unbind()
{
	glBindVertexArray(0);
}

vertex_array::~vertex_array()
{
	glDeleteVertexArrays(1, &id);
}
