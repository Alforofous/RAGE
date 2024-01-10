#pragma once

#include "glad/glad.h"
#include <string>

class GLB_utilities
{
public:
	static GLsizeiptr gl_data_type_size(GLenum gl_type);
	static GLsizeiptr get_attribute_component_count(std::string attribute_type);
	static GLenum component_type_to_gl_type(int glb_component_type);
	static int gl_type_to_component_type(GLenum gl_type);
	static std::string gl_data_type_to_string(GLenum gl_type);
};