#include "RAGE_primitive.hpp"
#include <iostream>

RAGE_primitive::RAGE_primitive()
{
	this->vertex_array_object = NULL;
	this->interleaved_vertex_buffer_object = NULL;
	this->element_buffer_object = NULL;
}

bool RAGE_primitive::interleave_vbos()
{
	if (this->non_interleaved_vertex_buffer_objects.size() == 0)
		return (false);
	else if (this->non_interleaved_vertex_buffer_objects.size() == 1)
	{
		this->interleaved_vertex_buffer_object = this->non_interleaved_vertex_buffer_objects.begin()->second;
		return (true);
	}

	size_t max_vertex_count = 0;
	for (std::pair<const std::string, buffer_object *> &pair : non_interleaved_vertex_buffer_objects)
		max_vertex_count = std::max(max_vertex_count, pair.second->get_vertex_count());

	buffer_object *interleaved_vbo = new (std::nothrow) buffer_object(GL_ARRAY_BUFFER, GL_NONE, NULL, 0);
	if (interleaved_vbo == NULL)
		return (false);
	for (size_t i = 0; i < max_vertex_count; i++)
	{
		for (std::pair<const std::string, buffer_object *> &pair : non_interleaved_vertex_buffer_objects)
		{
			buffer_object *vbo = pair.second;
			size_t component_byte_size = vbo->get_byte_size() / vbo->get_vertex_count();
			if (i < vbo->get_vertex_count())
				interleaved_vbo->push_back((uint8_t *)vbo->get_data() + i * component_byte_size, component_byte_size);
			else
				interleaved_vbo->push_empty(component_byte_size);
		}
	}
	if (this->interleaved_vertex_buffer_object != NULL)
		delete this->interleaved_vertex_buffer_object;
	this->interleaved_vertex_buffer_object = interleaved_vbo;
	return (true);
}

bool RAGE_primitive::init(GLfloat *vertices, GLuint *indices, GLsizeiptr vertices_size, GLsizeiptr indices_size)
{
	try
	{
		if (this->vertex_array_object == NULL)
			this->vertex_array_object = new vertex_array();
		this->vertex_array_object->bind();

		if (this->interleaved_vertex_buffer_object == NULL)
			this->interleaved_vertex_buffer_object = new buffer_object(GL_ARRAY_BUFFER, GL_NONE, vertices, vertices_size);
		else
			this->interleaved_vertex_buffer_object->update_data(vertices, vertices_size);
		if (this->element_buffer_object == NULL)
			this->element_buffer_object = new buffer_object(GL_ELEMENT_ARRAY_BUFFER, GL_NONE, indices, indices_size);
		else
			this->element_buffer_object->update_data(indices, indices_size);

		this->vertex_array_object->link_attributes(*this->interleaved_vertex_buffer_object, 0, 3, GL_FLOAT, 7 * sizeof(GLfloat), (void *)0);
		this->vertex_array_object->link_attributes(*this->element_buffer_object, 1, 4, GL_FLOAT, 7 * sizeof(GLfloat), (void *)(3 * sizeof(GLfloat)));
		this->vertex_array_object->unbind();
		this->interleaved_vertex_buffer_object->unbind();
		this->element_buffer_object->unbind();

		this->indices_count = (GLuint)(indices_size / sizeof(GLuint));
		return (true);
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error initializing primitive: " << e.what() << std::endl;
		return (false);
	}
}

void RAGE_primitive::draw()
{
	this->vertex_array_object->bind();
	this->element_buffer_object->bind();
	glDrawElements(GL_TRIANGLES, this->indices_count, GL_UNSIGNED_INT, 0);
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

	for (std::pair<const std::string, buffer_object *> &pair : non_interleaved_vertex_buffer_objects)
	{
		delete pair.second;
		pair.second = nullptr;
	}
	non_interleaved_vertex_buffer_objects.clear();
}
