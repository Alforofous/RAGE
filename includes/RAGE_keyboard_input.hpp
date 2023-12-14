#pragma once

#include "RAGE.hpp"
#include <unordered_map>

class RAGE_keyboard_input
{
public:
	std::unordered_map<int, bool> pressed_keys;
private:
};