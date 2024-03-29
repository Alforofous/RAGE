#include "RAGE_primitive.hpp"
#include "loaders/GLB_utilities.hpp"
#include "loaders/GLB_attribute_buffer.hpp"
#include <iostream>

RAGE_primitive::RAGE_primitive(const std::string &name)
{
	this->name = name;
	this->vertex_array_object = NULL;
	this->interleaved_vertex_buffer_object = NULL;
	this->element_buffer_object = NULL;
}

bool RAGE_primitive::interleave_vbos()
{
	if (this->attribute_buffers.size() == 0)
		return (false);

	GLsizeiptr max_vertex_count = 0;
	for (size_t i = 0; i < this->attribute_buffers.size(); i++)
	{
		GLB_attribute_buffer *attribute_buffer = this->attribute_buffers[i];
		if (this->attribute_buffers[i]->get_vertex_count() > max_vertex_count)
			max_vertex_count = this->attribute_buffers[i]->get_vertex_count();
	}

	buffer_object *interleaved_vbo = new (std::nothrow) buffer_object();
	if (interleaved_vbo == NULL)
		return (false);
	for (GLsizeiptr j = 0; j < max_vertex_count; j++)
	{
		for (size_t i = 0; i < this->attribute_buffers.size(); i++)
		{
			GLB_attribute_buffer *attribute_buffer = this->attribute_buffers[i];
			for (GLsizeiptr k = 0; k < attribute_buffer->get_component_count(); k++)
			{
				GLsizeiptr attribute_type_byte_size = GLB_utilities::gl_data_type_size(attribute_buffer->get_data_type());

				GLsizeiptr offset = j * attribute_buffer->get_component_count() + k;
				uint8_t *start = (uint8_t *)attribute_buffer->get_data() + offset * attribute_type_byte_size;
				uint8_t *end = (uint8_t *)attribute_buffer->get_data() + (offset + 1) * attribute_type_byte_size;
				if (j < attribute_buffer->get_vertex_count())
				{
					interleaved_vbo->data.insert(interleaved_vbo->data.end(), start, end);
				}
				else
				{
					interleaved_vbo->data.insert(interleaved_vbo->data.end(), attribute_type_byte_size, 0);
				}
			}
		}
	}
	interleaved_vbo->update_gl_buffer();
	if (this->vertex_array_object != NULL)
		delete this->vertex_array_object;
	this->vertex_array_object = new (std::nothrow) vertex_array();
	if (this->vertex_array_object == NULL)
		return (false);

	GLsizei stride = 0;
	for (size_t i = 0; i < this->attribute_buffers.size(); i++)
	{
		GLB_attribute_buffer *attribute_buffer = this->attribute_buffers[i];
		stride += (GLsizei)(GLB_utilities::gl_data_type_size(attribute_buffer->get_data_type()) * attribute_buffer->get_component_count());
	}

	GLsizeiptr offset = 0;
	for (size_t i = 0; i < this->attribute_buffers.size(); i++)
	{
		GLB_attribute_buffer *attribute_buffer = this->attribute_buffers[i];

		GLsizeiptr attribute_type_byte_size = GLB_utilities::gl_data_type_size(attribute_buffer->get_data_type());
		GLuint layout = GLB_utilities::get_attribute_layout_from_attribute_string(attribute_buffer->get_name());
		this->vertex_array_object->bind();
		interleaved_vbo->bind();
		this->element_buffer_object->bind();
		this->vertex_array_object->link_attributes(interleaved_vbo, layout, (GLuint)attribute_buffer->get_component_count(), attribute_buffer->get_data_type(), attribute_buffer->get_normalized(), stride, (void *)offset, attribute_buffer->get_name());
		this->vertex_array_object->unbind();
		interleaved_vbo->unbind();
		this->element_buffer_object->unbind();
		offset += attribute_type_byte_size * attribute_buffer->get_component_count();
	}
	if (this->interleaved_vertex_buffer_object != NULL)
		delete this->interleaved_vertex_buffer_object;
	this->interleaved_vertex_buffer_object = interleaved_vbo;
	return (true);
}

void RAGE_primitive::draw()
{
	this->interleaved_vertex_buffer_object->bind();
	this->vertex_array_object->bind();
	this->element_buffer_object->bind();
	glDrawElements(GL_TRIANGLES, this->indices_count, this->element_buffer_object->get_data_type(), (void *)0);
	this->interleaved_vertex_buffer_object->unbind();
	this->element_buffer_object->unbind();
	this->vertex_array_object->unbind();
}

bool RAGE_primitive::is_initialized()
{
	bool result;

	result |= this->interleaved_vertex_buffer_object != NULL;
	result |= this->vertex_array_object != NULL;
	result |= this->element_buffer_object != NULL;
	return (result);
}

RAGE_primitive::~RAGE_primitive()
{
	if (this->vertex_array_object != NULL)
		delete this->vertex_array_object;
	if (this->interleaved_vertex_buffer_object != NULL)
		delete this->interleaved_vertex_buffer_object;
	if (this->element_buffer_object != NULL)
		delete this->element_buffer_object;

	for (size_t i = 0; i < this->attribute_buffers.size(); i++)
	{
		delete this->attribute_buffers[i];
	}
}