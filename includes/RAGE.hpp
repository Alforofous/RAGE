#pragma once
#include "GLFW/glfw3.h"
#include "RAGE_window.hpp"

class RAGE
{
public:
	RAGE_window	*window;
	RAGE();
	void exit_program(char *exit_message, int exit_code);
private:

};