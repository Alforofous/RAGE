#pragma once

class RAGE_scene_view;
class RAGE;

class RAGE_gui
{
public:
	RAGE_gui(RAGE *rage);
	~RAGE_gui();
	void draw(RAGE *rage);
	void draw_fps_graph(RAGE *rage);
	void draw_scene_view(RAGE *rage);
private:
	std::vector<float> frames;
	GLuint framebuffer;
	GLuint depthbuffer;
	GLuint texture;
};