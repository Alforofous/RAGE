#pragma once

#include <unordered_map>

class RAGE_keyboard_input
{
public:
	void signal(void *param, int key);
	std::unordered_map<int, bool> pressed_keys;
private:
};