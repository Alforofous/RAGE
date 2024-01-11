#pragma once

#include "glad/glad.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>

#define POSITION_LAYOUT 0
#define NORMAL_LAYOUT 1
#define TEXCOORD_0_LAYOUT 2
#define COLOR_0_LAYOUT 3
#define TEXCOORD_1_LAYOUT 4
#define JOINTS_0_LAYOUT 5
#define WEIGHTS_0_LAYOUT 6
#define TANGENT_LAYOUT 7

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