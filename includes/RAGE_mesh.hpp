#pragma once

#include "RAGE.hpp"

class RAGE_mesh
{
public:
	int LoadGLB(const char *path);
private:
	GLfloat *vertices;
	GLuint *indices;
	GLsizeiptr vertice_size;
	GLsizeiptr indices_size;
};