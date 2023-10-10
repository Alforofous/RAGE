#ifndef SHADER_H
#define SHADER_H

#include "glad/glad.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

class RAGE_shader
{
public:
	GLuint	hProgram;
	RAGE_shader(const std::string& filePathVertexShader, const std::string& filePathFragmentShader);
	~RAGE_shader();
	std::map<std::string, int> variable_location;
	void InitVariableLocations();
private:
	std::string ReadFile(const std::string& filePath);
	GLuint CompileShader(GLuint type, const std::string& source);
	void CreateShader(const std::string& vertexShader, const std::string& fragmentShader);
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
	if (vs == 0 || fs == 0) { return; }

	hProgram = glCreateProgram();

	glAttachShader(hProgram, vs);
	glAttachShader(hProgram, fs);

	glLinkProgram(hProgram);

	GLint result;
	glGetShaderiv(hProgram, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE)
	{
		int length;
		glGetShaderiv(hProgram, GL_INFO_LOG_LENGTH, &length);
		char* infoLog = (char*)malloc(length * sizeof(char));
		glGetShaderInfoLog(hProgram, length, &length, infoLog);
		std::cout << "Failed to link vertex and fragment shader!"
			<< std::endl;
		std::cout << infoLog << std::endl;
		glDeleteProgram(hProgram);
		return;
	}
	glValidateProgram(hProgram);

	glDeleteShader(vs);
	glDeleteShader(fs);
}

//Initializes all the variables that shader uses and maps them into map<string, int>
inline void RAGE_shader::InitVariableLocations()
{
	char	*variable_names[] =
	{
		"u_resolution",
		"u_camera_position",
		"u_camera_direction"
	};
	for each (char * variable_name in variable_names)
	{
		variable_location[variable_name] = glGetUniformLocation(hProgram, variable_name);
	}
}
#endif