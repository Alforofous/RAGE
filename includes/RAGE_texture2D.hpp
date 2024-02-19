#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#include "glad/glad.h"

class RAGE_texture2D
{
public:
	RAGE_texture2D();
	RAGE_texture2D(const char *path);
	~RAGE_texture2D();

	bool load_gl_texture(const char *path);
	bool load_texture_data(const char *path);
	bool create_empty_texture(int width, int height, int channel_count = 3);

	GLuint get_id();
	int get_width();
	int get_height();
	int get_channel_count();
	uint8_t *get_data();
private:

	void bind();
	void unbind();

	GLuint id;
	int width;
	int height;
	int channel_count;
	uint8_t *data;
};