#pragma once

#include "nanogui/nanogui.h"
using namespace nanogui;

class RAGE_scene_view : public nanogui::GLCanvas
{
public:
	RAGE_scene_view(Widget *parent);
	virtual void drawGL() override;
private:
	GLShader	*base_shader;
	Vector3f	mRotation;
};
