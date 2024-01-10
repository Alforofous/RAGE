#include "loaders/GLB_utilities.hpp"
#include "RAGE_shader.hpp"

GLsizeiptr GLB_utilities::gl_data_type_size(GLenum gl_type)
{
	if (gl_type == GL_BYTE)
		return (sizeof(GLbyte));
	else if (gl_type == GL_UNSIGNED_BYTE)
		return (sizeof(GLubyte));
	else if (gl_type == GL_SHORT)
		return (sizeof(GLshort));
	else if (gl_type == GL_UNSIGNED_SHORT)
		return (sizeof(GLushort));
	else if (gl_type == GL_INT)
		return (sizeof(GLint));
	else if (gl_type == GL_UNSIGNED_INT)
		return (sizeof(GLuint));
	else if (gl_type == GL_HALF_FLOAT)
		return (sizeof(GLhalf));
	else if (gl_type == GL_FLOAT)
		return (sizeof(GLfloat));
	else if (gl_type == GL_DOUBLE)
		return (sizeof(GLdouble));
	else
		return (0);
}

GLuint GLB_utilities::get_attribute_layout_from_attribute_string(std::string attribute_name)
{
	if (attribute_name == "POSITION")
		return (POSITION_LAYOUT);
	else if (attribute_name == "COLOR_0")
		return (COLOR_LAYOUT);
	else if (attribute_name == "NORMAL")
		return (NORMAL_LAYOUT);
	else
		throw std::runtime_error("Unknown attribute name: " + attribute_name);
	return (0);
}

GLsizeiptr GLB_utilities::get_attribute_component_count(std::string attribute_type)
{
	if (attribute_type == "SCALAR")
		return (1);
	else if (attribute_type == "VEC2")
		return (2);
	else if (attribute_type == "VEC3")
		return (3);
	else if (attribute_type == "VEC4")
		return (4);
	else if (attribute_type == "MAT2")
		return (4);
	else if (attribute_type == "MAT3")
		return (9);
	else if (attribute_type == "MAT4")
		return (16);
	else
		return (0);
}

GLenum GLB_utilities::component_type_to_gl_type(int glb_component_type)
{
	if (glb_component_type == 5120)
		return (GL_BYTE);
	else if (glb_component_type == 5121)
		return (GL_UNSIGNED_BYTE);
	else if (glb_component_type == 5122)
		return (GL_SHORT);
	else if (glb_component_type == 5123)
		return (GL_UNSIGNED_SHORT);
	else if (glb_component_type == 5124)
		return (GL_INT);
	else if (glb_component_type == 5125)
		return (GL_UNSIGNED_INT);
	else if (glb_component_type == 5131)
		return (GL_HALF_FLOAT);
	else if (glb_component_type == 5126)
		return (GL_FLOAT);
	else if (glb_component_type == 5127)
		return (GL_DOUBLE);
	else
		return (GL_NONE);
}

int GLB_utilities::gl_type_to_component_type(GLenum gl_type)
{
	if (gl_type == GL_BYTE)
		return (5120);
	else if (gl_type == GL_UNSIGNED_BYTE)
		return (5121);
	else if (gl_type == GL_SHORT)
		return (5122);
	else if (gl_type == GL_UNSIGNED_SHORT)
		return (5123);
	else if (gl_type == GL_INT)
		return (5124);
	else if (gl_type == GL_UNSIGNED_INT)
		return (5125);
	else if (gl_type == GL_HALF_FLOAT)
		return (5131);
	else if (gl_type == GL_FLOAT)
		return (5126);
	else if (gl_type == GL_DOUBLE)
		return (5127);
	else
		return (-1);
}

std::string GLB_utilities::gl_data_type_to_string(GLenum gl_data_type)
{
	if (gl_data_type == GL_BYTE)
		return ("GL_BYTE");
	else if (gl_data_type == GL_UNSIGNED_BYTE)
		return ("GL_UNSIGNED_BYTE");
	else if (gl_data_type == GL_SHORT)
		return ("GL_SHORT");
	else if (gl_data_type == GL_UNSIGNED_SHORT)
		return ("GL_UNSIGNED_SHORT");
	else if (gl_data_type == GL_INT)
		return ("GL_INT");
	else if (gl_data_type == GL_UNSIGNED_INT)
		return ("GL_UNSIGNED_INT");
	else if (gl_data_type == GL_HALF_FLOAT)
		return ("GL_HALF_FLOAT");
	else if (gl_data_type == GL_FLOAT)
		return ("GL_FLOAT");
	else if (gl_data_type == GL_DOUBLE)
		return ("GL_DOUBLE");
	else
		return ("GL_NONE");
}