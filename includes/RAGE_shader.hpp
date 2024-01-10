#pragma once

#include "glad/glad.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>

#define POSITION_LAYOUT 0
#define COLOR_LAYOUT 1
#define NORMAL_LAYOUT 2

class RAGE_shader
{
public:
	RAGE_shader(const std::string &filePathVertexShader, const std::string &filePathFragmentShader);
	~RAGE_shader();

	void init_variable_locations();

	GLuint hProgram;
	std::map<std::string, int> variable_location;

private:
	std::string ReadFile(const std::string &filePath);
	GLuint CompileShader(GLuint type, const std::string &source);
	void CreateShader(const std::string &vertexShader, const std::string &fragmentShader);
};