#include <ctime>

double clockToMilliseconds(clock_t ticks)
{
	return (ticks / (double)CLOCKS_PER_SEC);
}
