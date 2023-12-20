#pragma once

#include "RAGE_object.hpp"

#define CUBE_VERTEX_COUNT 8
#define CUBE_TRIANGLE_COUNT 12

class RAGE_primitive_objects
{
public:
	static RAGE_object *create_cube(float width = 1.0f, float height = 1.0f, float depth = 1.0f);
};