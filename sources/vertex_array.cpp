#include "vertex_array.hpp"
#include "loaders/GLB_utilities.hpp"

#include <string>

vertex_array::vertex_array()
{
	glGenVertexArrays(1, &id);
}

void vertex_array::link_attributes(buffer_object *vertex_buffer_object, GLuint layout, GLuint component_count, GLenum gl_data_type, GLboolean normalized, GLsizei stride, void *offset, std::string name)
{
	linked_attributes[layout] = {vertex_buffer_object, component_count, gl_data_type, stride, offset, name};

	vertex_buffer_object->bind();
	glVertexAttribPointer(layout, component_count, gl_data_type, normalized, stride, offset);
	glEnableVertexAttribArray(layout);
	vertex_buffer_object->unbind();
}

template <typename T>
static std::string get_attribute_data_with_data_type(void *data, size_t size, GL_attribute attribute)
{
	std::string result;
	uint8_t *start = (uint8_t *)data + (size_t)attribute.offset;
	uint8_t *end = (uint8_t *)data + size;
	int i = 0;
	GLenum data_type = attribute.data_type;
	GLsizeiptr data_type_size = GLB_utilities::gl_data_type_size(data_type);
	for (uint8_t *typed_data = start; typed_data < end; typed_data += attribute.stride)
	{
		result += "offset: " + std::to_string((size_t)typed_data - (size_t)data) + " ";
		for (size_t j = 0; j < attribute.component_count; j++)
		{
			result += std::to_string(*((T *)(typed_data + j * data_type_size))) + " ";
		}

		result += "\n";
		i += 1;
	}
	return (result);
}

std::string vertex_array::get_linked_attribute_data(GLuint layout)
{
	if (linked_attributes.find(layout) == linked_attributes.end())
		return ("No attribute found for layout " + std::to_string(layout) + ".");
	GL_attribute linked_attribute = linked_attributes[layout];
	std::string result = linked_attribute.name + " ATTRIBUTE AT LAYOUT " + std::to_string(layout) + ":\n";
	void *data = linked_attribute.vertex_buffer_object->get_data();
	if (data == NULL)
		return ("Data is NULL.");
	GLuint byte_size = linked_attribute.vertex_buffer_object->get_byte_size();
	if (linked_attribute.data_type == GL_BYTE)
		result += get_attribute_data_with_data_type<GLbyte>(data, byte_size, linked_attribute);
	else if (linked_attribute.data_type == GL_UNSIGNED_BYTE)
		result += get_attribute_data_with_data_type<GLubyte>(data, byte_size, linked_attribute);
	else if (linked_attribute.data_type == GL_SHORT)
		result += get_attribute_data_with_data_type<GLshort>(data, byte_size, linked_attribute);
	else if (linked_attribute.data_type == GL_UNSIGNED_SHORT)
		result += get_attribute_data_with_data_type<GLushort>(data, byte_size, linked_attribute);
	else if (linked_attribute.data_type == GL_INT)
		result += get_attribute_data_with_data_type<GLint>(data, byte_size, linked_attribute);
	else if (linked_attribute.data_type == GL_UNSIGNED_INT)
		result += get_attribute_data_with_data_type<GLuint>(data, byte_size, linked_attribute);
	else if (linked_attribute.data_type == GL_FLOAT)
		result += get_attribute_data_with_data_type<GLfloat>(data, byte_size, linked_attribute);
	else if (linked_attribute.data_type == GL_DOUBLE)
		result += get_attribute_data_with_data_type<GLdouble>(data, byte_size, linked_attribute);
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
