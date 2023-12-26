#pragma once

#include "glad/glad.h"

class RAGE_scene_view
{
public:
	RAGE_scene_view();
	~RAGE_scene_view();
	void draw(RAGE *rage);
	bool is_focused();
private:
	GLuint framebuffer;
	GLuint depthbuffer;
	GLuint texture;
	glm::ivec2 window_size;
	bool focused;
};
