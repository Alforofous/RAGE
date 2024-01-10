#include "loaders/GLB_attribute_buffer.hpp"
#include "loaders/GLB_loader.hpp"
#include <cstddef>
#include <iostream>

GLB_attribute_buffer::GLB_attribute_buffer(GLvoid *glb_buffer, GLsizeiptr byte_offset, GLsizeiptr count, std::string name, GLsizeiptr component_count, GLenum gl_data_type, GLboolean normalized, GLsizei stride)
{
	this->name = name;
	this->component_count = component_count;
	this->gl_data_type = gl_data_type;
	this->normalized = normalized;
	this->stride = stride;
	this->size = count * GLB_loader::sizeof_gl_data_type(gl_data_type);
	if (this->size == 0)
		throw std::runtime_error("GLB_attribute_buffer::GLB_attribute_buffer(): size == 0 for attribute: " + name);

	if (gl_data_type == GL_BYTE)
		this->data = new GLbyte[size];
	else if (gl_data_type == GL_UNSIGNED_BYTE)
		this->data = new GLubyte[size];
	else if (gl_data_type == GL_SHORT)
		this->data = new GLshort[size];
	else if (gl_data_type == GL_UNSIGNED_SHORT)
		this->data = new GLushort[size];
	else if (gl_data_type == GL_INT)
		this->data = new GLint[size];
	else if (gl_data_type == GL_UNSIGNED_INT)
		this->data = new GLuint[size];
	else if (gl_data_type == GL_FLOAT)
		this->data = new GLfloat[size];
	else if (gl_data_type == GL_DOUBLE)
		this->data = new GLdouble[size];
	else
		throw std::runtime_error("GLB_attribute_buffer::GLB_attribute_buffer(): unknown gl_data_type for attribute: " + name);

	memcpy(this->data, (GLubyte *)glb_buffer + byte_offset, this->size);
}

void GLB_attribute_buffer::print_data()
{
	GLsizeiptr data_type_size = GLB_loader::sizeof_gl_data_type(this->gl_data_type);
	if (data_type_size == 0)
		return;

	int stride = 0;
	for (uint8_t *ptr = (uint8_t *)this->data; ptr < (uint8_t *)this->data + this->size; ptr += data_type_size)
	{
		if (this->gl_data_type == GL_BYTE)
			std::cout << (int)*(int8_t *)ptr;
		else if (this->gl_data_type == GL_UNSIGNED_BYTE)
			std::cout << (int)*(uint8_t *)ptr;
		else if (this->gl_data_type == GL_SHORT)
			std::cout << *(int16_t *)ptr;
		else if (this->gl_data_type == GL_UNSIGNED_SHORT)
			std::cout << *(uint16_t *)ptr;
		else if (this->gl_data_type == GL_INT)
			std::cout << *(int32_t *)ptr;
		else if (this->gl_data_type == GL_UNSIGNED_INT)
			std::cout << *(uint32_t *)ptr;
		else if (this->gl_data_type == GL_FLOAT)
			std::cout << *(float *)ptr;
		else if (this->gl_data_type == GL_DOUBLE)
			std::cout << *(double *)ptr;
		else
			std::cout << "Unknown data type";
		if (stride % this->stride == this->stride - 1)
			std::cout << std::endl;
		else
			std::cout << " ";
		stride += 1;
	}

}

GLB_attribute_buffer::~GLB_attribute_buffer()
{
	delete[] static_cast<uint8_t *>(this->data);
}