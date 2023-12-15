#pragma once

#include "RAGE.hpp"

class RAGE_mesh
{
public:
	int LoadGLB(const char *path);
	GLfloat *vertices;
	GLuint *indices;
	GLsizeiptr vertices_size;
	GLsizeiptr indices_size;
private:
};