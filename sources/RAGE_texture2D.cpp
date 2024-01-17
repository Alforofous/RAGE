#include "RAGE_texture2D.hpp"

#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

RAGE_texture2D::RAGE_texture2D()
{
	glGenTextures(1, &this->id);
}

RAGE_texture2D::RAGE_texture2D(const char *path)
{
	glGenTextures(1, &this->id);
	if (this->load(path) == false)
		throw std::runtime_error("Failed to load texture");
}

bool RAGE_texture2D::load(const char *path)
{
	if (path == NULL)
		return (false);
	this->bind();

	stbi_set_flip_vertically_on_load(true);
	this->data = stbi_load(path, &this->width, &this->height, &this->channel_count, 0);
	if (this->data)
	{
		GLenum format;
		if (this->channel_count == 1)
			format = GL_RED;
		else if (this->channel_count == 3)
			format = GL_RGB;
		else if (this->channel_count == 4)
			format = GL_RGBA;
		glTexImage2D(GL_TEXTURE_2D, 0, format, this->width, this->height, 0, format, GL_UNSIGNED_BYTE, this->data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		return (false);
	}
	stbi_image_free(this->data);
	return (true);
}

void RAGE_texture2D::bind()
{
	glBindTexture(GL_TEXTURE_2D, this->id);
}

void RAGE_texture2D::unbind()
{
	glBindTexture(GL_TEXTURE_2D, 0);
}

int RAGE_texture2D::get_width()
{
	return (this->width);
}

int RAGE_texture2D::get_height()
{
	return (this->height);
}

int RAGE_texture2D::get_channel_count()
{
	return (this->channel_count);
}

uint8_t *RAGE_texture2D::get_data()
{
	return (this->data);
}

GLuint RAGE_texture2D::get_id()
{
	return (this->id);
}

RAGE_texture2D::~RAGE_texture2D()
{
	glDeleteTextures(1, &this->id);
}