#include "buffer_object.hpp"
#include "loaders/GLB_utilities.hpp"
#include <iostream>

buffer_object::buffer_object(GLenum buffer_type, GLenum data_type, void *data, GLsizeiptr byte_size)
{
	this->init(buffer_type, data_type, data, byte_size);
}

buffer_object::buffer_object()
{
	this->init(GL_ARRAY_BUFFER, GL_NONE, NULL, 0);
}

void buffer_object::init(GLenum buffer_type, GLenum data_type, void *data, GLsizeiptr byte_size)
{
	this->buffer_type = buffer_type;
	this->data_type = data_type;
	this->data.assign((uint8_t *)data, (uint8_t *)data + byte_size);
	this->byte_size = byte_size;
	GLsizeiptr gl_data_type_size = GLB_utilities::gl_data_type_size(data_type);
	if (gl_data_type_size != 0)
		this->vertex_count = byte_size / gl_data_type_size;
	else
		this->vertex_count = 0;
	glGenBuffers(1, &this->id);
	glBindBuffer(this->buffer_type, this->id);
	glBufferData(this->buffer_type, this->byte_size, this->data.data(), GL_STATIC_DRAW);
}

void buffer_object::update_gl_buffer()
{
	glBindBuffer(this->buffer_type, this->id);
	this->byte_size = this->data.size();
	glBufferData(this->buffer_type, this->byte_size, this->data.data(), GL_STATIC_DRAW);
}

buffer_object *buffer_object::create_from_glb_buffer(GLenum gl_buffer_type, std::vector<char> &glb_buffer, size_t byte_offset, size_t count, GLenum data_type)
{
	void *gl_buffer_data;
	buffer_object *gl_buffer_object;
	GLsizeiptr size = count * GLB_utilities::gl_data_type_size(data_type);
	if (size == 0)
		return (NULL);

	gl_buffer_data = &glb_buffer[byte_offset];
	gl_buffer_object = new buffer_object(gl_buffer_type, data_type, (void *)gl_buffer_data, size);
	return (gl_buffer_object);
}

std::string buffer_object::get_buffer_data(int component_count)
{
	GLsizeiptr data_type_size = GLB_utilities::gl_data_type_size(this->data_type);
	if (data_type_size == 0)
		data_type_size = 1;

	int count = 0;
	std::string buffer_data = "";
	for (uint8_t *ptr = (uint8_t *)this->data.data(); ptr < (uint8_t *)this->data.data() + this->byte_size; ptr += data_type_size)
	{
		if (this->data_type == GL_BYTE)
			buffer_data += std::to_string((int)*(int8_t *)ptr);
		else if (this->data_type == GL_UNSIGNED_BYTE)
			buffer_data += std::to_string((int)*(uint8_t *)ptr);
		else if (this->data_type == GL_SHORT)
			buffer_data += std::to_string(*(int16_t *)ptr);
		else if (this->data_type == GL_UNSIGNED_SHORT)
			buffer_data += std::to_string(*(uint16_t *)ptr);
		else if (this->data_type == GL_INT)
			buffer_data += std::to_string(*(int32_t *)ptr);
		else if (this->data_type == GL_UNSIGNED_INT)
			buffer_data += std::to_string(*(uint32_t *)ptr);
		else if (this->data_type == GL_FLOAT)
			buffer_data += std::to_string(*(float *)ptr);
		else if (this->data_type == GL_DOUBLE)
			buffer_data += std::to_string(*(double *)ptr);
		else
			buffer_data += "GL_NONE: [" + std::to_string((int)*(uint8_t *)ptr) + "]";
		if (count % component_count == component_count - 1)
			buffer_data += "\n";
		else
			buffer_data += " ";
		count += 1;
	}
	return (buffer_data);
}

std::string buffer_object::get_buffer_data(int component_count, GLenum data_type)
{
	GLsizeiptr data_type_size = GLB_utilities::gl_data_type_size(data_type);
	if (data_type_size == 0)
		data_type_size = 1;

	int count = 0;
	std::string buffer_data = "";
	for (uint8_t *ptr = (uint8_t *)this->data.data(); ptr < (uint8_t *)this->data.data() + this->byte_size; ptr += data_type_size)
	{
		if (data_type == GL_BYTE)
			buffer_data += std::to_string((int)*(int8_t *)ptr);
		else if (data_type == GL_UNSIGNED_BYTE)
			buffer_data += std::to_string((int)*(uint8_t *)ptr);
		else if (data_type == GL_SHORT)
			buffer_data += std::to_string(*(int16_t *)ptr);
		else if (data_type == GL_UNSIGNED_SHORT)
			buffer_data += std::to_string(*(uint16_t *)ptr);
		else if (data_type == GL_INT)
			buffer_data += std::to_string(*(int32_t *)ptr);
		else if (data_type == GL_UNSIGNED_INT)
			buffer_data += std::to_string(*(uint32_t *)ptr);
		else if (data_type == GL_FLOAT)
			buffer_data += std::to_string(*(float *)ptr);
		else if (data_type == GL_DOUBLE)
			buffer_data += std::to_string(*(double *)ptr);
		else
			buffer_data += "GL_NONE: [" + std::to_string((int)*(uint8_t *)ptr) + "]";
		if (count % component_count == component_count - 1)
			buffer_data += "\n";
		else
			buffer_data += " ";
		count += 1;
	}
	return (buffer_data);
}

void *buffer_object::get_data()
{
	return (this->data.data());
}

GLuint buffer_object::get_byte_size()
{
	return (this->byte_size);
}

size_t buffer_object::get_vertex_count()
{
	return (this->vertex_count);
}

GLenum buffer_object::get_data_type()
{
	return (this->data_type);
}

void buffer_object::bind()
{
	glBindBuffer(this->buffer_type, this->id);
}

void buffer_object::unbind()
{
	glBindBuffer(this->buffer_type, 0);
}

buffer_object::~buffer_object()
{
	glDeleteBuffers(1, &this->id);
}