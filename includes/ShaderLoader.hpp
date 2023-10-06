#ifndef SHADER_H
#define SHADER_H

#include "glad/glad.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

class ShaderLoader
{
public:
	ShaderLoader(const std::string& filePathVertexShader, const std::string& filePathFragmentShader);
	~ShaderLoader();
	GLuint hProgram;

private:
	std::string ReadFile(const std::string& filePath);
	GLuint CompileShader(GLuint type, const std::string& source);
	void CreateShader(const std::string& vertexShader, const std::string& fragmentShader);
};


inline 
ShaderLoader::ShaderLoader(const std::string& filePathVertexShader, const std::string& filePathFragmentShader)
{
	std::string vertexShader = ReadFile(filePathVertexShader);
	std::string fragmentShader = ReadFile(filePathFragmentShader);
	CreateShader(vertexShader, fragmentShader);
	glUseProgram(hProgram);
}


inline 
ShaderLoader::~ShaderLoader()
{
	if (hProgram != 0)
	{
		glDeleteProgram(hProgram);
	}
}

inline 
std::string ShaderLoader::ReadFile(const std::string& filePath) {
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

inline 
GLuint ShaderLoader::CompileShader(GLuint type, const std::string& source)
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
		char* infoLog = (char*)alloca(length * sizeof(char));
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

inline 
void ShaderLoader::CreateShader(const std::string& vertexShader, const std::string& fragmentShader)
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
		char* infoLog = (char*)alloca(length * sizeof(char));
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

#endif