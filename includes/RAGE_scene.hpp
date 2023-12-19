#pragma once

#include "RAGE_object.hpp"
#include <vector>

class RAGE_scene
{
public:
	RAGE_scene();
	~RAGE_scene();

	void loadScene(const char *sceneName);
	void renderScene();
	void updateScene(float deltaTime);
private:
	std::vector<RAGE_object> objects;
};