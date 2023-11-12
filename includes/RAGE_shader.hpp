#ifndef SHADER_H
#define SHADER_H

#include "glad/glad.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>

class RAGE_shader
{
public:
	GLuint						hProgram;
	std::map<std::string, int>	variable_location;

	RAGE_shader(const std::string& filePathVertexShader, const std::string& filePathFragmentShader);
	~RAGE_shader();
	void InitVariableLocations();
private:
	std::string	ReadFile(const std::string& filePath);
	GLuint		CompileShader(GLuint type, const std::string& source);
	void		CreateShader(const std::string& vertexShader, const std::string& fragmentShader);
};

inline RAGE_shader::RAGE_shader(const std::string& filePathVertexShader, const std::string& filePathFragmentShader)
{
	std::string vertexShader = ReadFile(filePathVertexShader);
	std::string fragmentShader = ReadFile(filePathFragmentShader);
	CreateShader(vertexShader, fragmentShader);
	glUseProgram(hProgram);
}

#include "RAGE.hpp"

inline RAGE_shader::~RAGE_shader()
{
	if (hProgram != 0)
	{
		glDeleteProgram(hProgram);
	}
}

inline std::string RAGE_shader::ReadFile(const std::string& filePath)
{
	std::ifstream fs(filePath, std::ios::in);

	if (!fs.is_open()) {
		std::cerr << "Could not read file " << filePath << ". File does not exist." << std::endl;
		return "";
	}

	std::stringstream buffer;
	std::string line;
	while (std::getline(fs, line))
	{
		buffer << line << "\n";
	}

	fs.close();
	return buffer.str();
}

inline GLuint RAGE_shader::CompileShader(GLuint type, const std::string& source)
{
	GLuint hShader = glCreateShader(type);

	const char* src = source.c_str();
	glShaderSource(hShader, 1, &src, nullptr);

	glCompileShader(hShader);

	GLint result;
	glGetShaderiv(hShader, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE)
	{
		int length;
		glGetShaderiv(hShader, GL_INFO_LOG_LENGTH, &length);
		char* infoLog = (char*)malloc(length * sizeof(char));
		glGetShaderInfoLog(hShader, length, &length, infoLog);
		std::cout << "Failed to compile shader!"
			<< (type == GL_VERTEX_SHADER ? "vertex" : "fragment")
			<< std::endl;
		std::cout << infoLog << std::endl;
		glDeleteShader(hShader);
		return static_cast<GLuint>(0);
	}
	return hShader;
}

inline void RAGE_shader::CreateShader(const std::string& vertexShader, const std::string& fragmentShader)
{
	GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexShader);
	GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);
	if (vs == 0 || fs == 0)
		return;

	hProgram = glCreateProgram();

	glAttachShader(hProgram, vs);
	glAttachShader(hProgram, fs);

	glLinkProgram(hProgram);

	GLint isLinked = 0;
	glGetProgramiv(hProgram, GL_LINK_STATUS, &isLinked);
	if (isLinked == GL_FALSE)
	{
		GLint maxLength = 0;
		glGetProgramiv(hProgram, GL_INFO_LOG_LENGTH, &maxLength);
		std::vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(hProgram, maxLength, &maxLength, &infoLog[0]);
		glDeleteProgram(hProgram);
		glDeleteShader(vs);
		glDeleteShader(fs);

		std::cout << "Failed to link vertex and fragment shader!" << std::endl;
		std::cout << &infoLog[0] << std::endl;
		return;
	}
	glValidateProgram(hProgram);

	glDeleteShader(vs);
	glDeleteShader(fs);
}

//Initializes all the variables that shader uses and maps them into map<string, int>
inline void RAGE_shader::InitVariableLocations()
{
	const char	*variable_names[] =
	{
		"u_resolution",
		"u_camera_position",
		"u_camera_direction"
	};
	for (int i = 0; i < 3; i++)
	{
		variable_location[variable_names[i]] = glGetUniformLocation(hProgram, variable_names[i]);
	}
}
#endif