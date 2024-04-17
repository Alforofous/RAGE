#include "RAGE_texture2D.hpp"

#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <algorithm>

RAGE_texture2D::RAGE_texture2D()
{
	this->data = NULL;
	glGenTextures(1, &this->id);
}

RAGE_texture2D::RAGE_texture2D(const char *path)
{
	this->data = NULL;
	glGenTextures(1, &this->id);
	if (this->load_gl_texture(path) == false)
		throw std::runtime_error("Failed to load texture " + std::string(path));
}

bool RAGE_texture2D::load_texture_data(const char *path)
{
	if (this->data != NULL)
		stbi_image_free(this->data);
	if (path == NULL)
		path = "";
	this->data = stbi_load(path, &this->width, &this->height, &this->channel_count, 0);
	if (this->data == NULL)
	{
		this->create_empty_texture(1, 1, 3);
		return (false);
	}
	return (true);
}

bool RAGE_texture2D::create_empty_texture(int width, int height, int channel_count)
{
	if (this->data != NULL)
		stbi_image_free(this->data);
	this->width = width;
	this->height = height;
	this->channel_count = channel_count;
	std::fill_n(this->data, width * height * channel_count, 0);
	return true;
}

bool RAGE_texture2D::load_gl_texture(const char *path)
{
	bool loaded_from_path = this->load_texture_data(path);
	this->bind();
	GLenum format = GL_RGB;
	if (this->channel_count == 1)
		format = GL_RED;
	else if (this->channel_count == 4)
		format = GL_RGBA;
	glTexImage2D(GL_TEXTURE_2D, 0, format, this->width, this->height, 0, format, GL_UNSIGNED_BYTE, this->data);
	glGenerateMipmap(GL_TEXTURE_2D);
	return (loaded_from_path);
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