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
	bool load(const char *path);

	void bind();
	void unbind();

	GLuint get_id();
	int get_width();
	int get_height();
	int get_channel_count();
	uint8_t *get_data();
private:
	GLuint id;
	int width;
	int height;
	int channel_count;
	uint8_t *data;
};