#include "buffer_object.hpp"
#include "loaders/RAGE_GLB_loader.hpp"
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
	this->data = data;
	this->byte_size = byte_size;
	GLsizeiptr gl_data_type_size = RAGE_GLB_loader::sizeof_gl_data_type(data_type);
	if (gl_data_type_size != 0)
		this->vertex_count = byte_size / gl_data_type_size;
	else
		this->vertex_count = 0;
	glGenBuffers(1, &this->id);
	glBindBuffer(this->buffer_type, this->id);
	glBufferData(this->buffer_type, this->byte_size, this->data, GL_STATIC_DRAW);
}

void buffer_object::push_back(void *data, GLsizeiptr byte_size)
{
	glBindBuffer(this->buffer_type, this->id);
	glBufferData(this->buffer_type, this->byte_size + byte_size, NULL, GL_STATIC_DRAW);
	glBufferSubData(this->buffer_type, this->byte_size, byte_size, data);
	this->byte_size += byte_size;
}

void buffer_object::push_empty(GLsizeiptr byte_size)
{
	glBindBuffer(this->buffer_type, this->id);
	glBufferData(this->buffer_type, this->byte_size + byte_size, NULL, GL_STATIC_DRAW);
	glBufferSubData(this->buffer_type, this->byte_size, byte_size, NULL);
	this->byte_size += byte_size;
}

void buffer_object::update_data(void *data, GLsizeiptr byte_size)
{
	glBindBuffer(this->buffer_type, this->id);
	glBufferSubData(this->buffer_type, 0, byte_size, data);
}

buffer_object *buffer_object::create_from_glb_buffer(GLenum gl_buffer_type, std::vector<char> &glb_buffer, size_t byte_offset, size_t count, GLenum data_type)
{
	void *gl_buffer_data;
	buffer_object *gl_buffer_object;
	GLsizeiptr size = count * RAGE_GLB_loader::sizeof_gl_data_type(data_type);
	if (size == 0)
		return (NULL);

	gl_buffer_data = &glb_buffer[byte_offset];
	gl_buffer_object = new buffer_object(gl_buffer_type, data_type, (void *)gl_buffer_data, size);
	return (gl_buffer_object);
}

void buffer_object::print_data(int stride_size)
{
	GLsizeiptr data_type_size = RAGE_GLB_loader::sizeof_gl_data_type(this->data_type);
	if (data_type_size == 0)
		return;

	int stride = 0;
	for (uint8_t *ptr = (uint8_t *)this->data; ptr < (uint8_t *)this->data + this->byte_size; ptr += data_type_size)
	{
		if (this->data_type == GL_BYTE)
			std::cout << (int)*(int8_t *)ptr;
		else if (this->data_type == GL_UNSIGNED_BYTE)
			std::cout << (int)*(uint8_t *)ptr;
		else if (this->data_type == GL_SHORT)
			std::cout << *(int16_t *)ptr;
		else if (this->data_type == GL_UNSIGNED_SHORT)
			std::cout << *(uint16_t *)ptr;
		else if (this->data_type == GL_INT)
			std::cout << *(int32_t *)ptr;
		else if (this->data_type == GL_UNSIGNED_INT)
			std::cout << *(uint32_t *)ptr;
		else if (this->data_type == GL_FLOAT)
			std::cout << *(float *)ptr;
		else if (this->data_type == GL_DOUBLE)
			std::cout << *(double *)ptr;
		else
			std::cout << "Unknown data type";
		if (stride % stride_size == stride_size - 1)
			std::cout << std::endl;
		else
			std::cout << " ";
		stride += 1;
	}
}

void *buffer_object::get_data()
{
	return (this->data);
}

GLuint buffer_object::get_byte_size()
{
	return (this->byte_size);
}

size_t buffer_object::get_vertex_count()
{
	return (this->vertex_count);
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