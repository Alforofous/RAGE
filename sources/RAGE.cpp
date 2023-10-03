#include "RAGE.hpp"

RAGE::RAGE()
{
	window = new RAGE_window();
}

void RAGE::exit_program(char *exit_message, int exit_code)
{
	delete window;
}
