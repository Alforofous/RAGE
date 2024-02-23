#include "RAGE.hpp"
#include "RAGE_shader.hpp"
#include "RAGE_texture2D.hpp"

RAGE_shader::RAGE_shader(const char *vertex_path, const char *fragment_path)
{
	this->vertex_path = vertex_path;
	this->fragment_path = fragment_path;
	std::string vertex_string = read_file(vertex_path);
	std::string fragment_string = read_file(fragment_path);
	create(vertex_string.c_str(), fragment_string.c_str());
}

std::string RAGE_shader::read_file(const char *file_path)
{
	std::ifstream file_stream(file_path, std::ios::in);

	if (!file_stream.is_open())
	{
		std::cerr << "Could not read file " << file_path << ". File does not exist." << std::endl;
		return ("");
	}

	std::stringstream buffer;
	std::string line;
	while (std::getline(file_stream, line))
	{
		buffer << line << "\n";
	}

	file_stream.close();
	return (buffer.str());
}

GLuint RAGE_shader::compile(GLuint type, const char *source)
{
	GLuint shader = glCreateShader(type);

	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	GLint result;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE)
	{
		int length;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
		char* info_log = (char*)malloc(length * sizeof(char));
		glGetShaderInfoLog(shader, length, &length, info_log);
		std::cout << "Failed to compile shader!"
			<< (type == GL_VERTEX_SHADER ? "vertex" : "fragment")
			<< std::endl;
		std::cout << info_log << std::endl;
		glDeleteShader(shader);
		return static_cast<GLuint>(0);
	}
	return (shader);
}

void RAGE_shader::create(const char *vertex_string, const char *fragment_string)
{
	GLuint vertex_shader = compile(GL_VERTEX_SHADER, vertex_string);
	GLuint fragment_shader = compile(GL_FRAGMENT_SHADER, fragment_string);
	if (vertex_shader == 0 || fragment_shader == 0)
		return;

	program = glCreateProgram();

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);

	glLinkProgram(program);

	GLint is_linked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &is_linked);
	if (is_linked == GL_FALSE)
	{
		GLint max_length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &max_length);
		std::vector<GLchar> infoLog(max_length);
		glGetProgramInfoLog(program, max_length, &max_length, &infoLog[0]);
		glDeleteProgram(program);
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);

		std::cout << "Failed to link vertex and fragment shader!" << std::endl;
		std::cout << &infoLog[0] << std::endl;
		return;
	}
	glValidateProgram(program);

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
}

GLint RAGE_shader::init_uniform_location(const char *variable_name)
{
	if (variable_name == NULL)
	{
		return (-1);
	}
	RAGE_uniform uniform;
	uniform.location = glGetUniformLocation(this->program, variable_name);
	this->uniforms[variable_name] = uniform;
	return (uniform.location);
}

GLint RAGE_shader::init_uniform(const char *variable_name, GLenum type, std::function<void(GLint location, void *content)> update)
{
	if (variable_name == NULL)
	{
		std::cout << "Warning: variable_name is NULL" << std::endl;
		return (-1);
	}
	GLint location = init_uniform_location(variable_name);
	if (location == -1)
	{
		std::cout << "Warning: Failed to locate uniform variable " << variable_name << std::endl;
		return (-1);
	}
	RAGE_uniform uniform;
	uniform.location = location;
	uniform.type = type;
	uniform.update = update;
	this->uniforms[variable_name] = uniform;
	return (uniform.location);
}

void RAGE_shader::update_uniforms(void *content)
{
	glUseProgram(this->program);
	for (auto& [key, uniform] : this->uniforms)
	{
		if (uniform.update != NULL)
			uniform.update(uniform.location, content);
	}
}

void RAGE_shader::update_uniform(const char *variable_name, void *content)
{
	glUseProgram(this->program);
	RAGE_uniform uniform = this->uniforms[variable_name];
	if (uniform.update != NULL)
		uniform.update(uniform.location, content);
}

void RAGE_shader::init_RAGE_shaders(void *content)
{
	RAGE *rage;

	rage = (RAGE *)content;
	rage->shader = new RAGE_shader((rage->executable_path + "/shaders/vertex_test.glsl").c_str(),
								   (rage->executable_path + "/shaders/fragment_test.glsl").c_str());
	rage->shader->init_uniform("u_view_matrix", GL_FLOAT_MAT4, update_view_matrix);
	rage->shader->init_uniform("u_perspective_matrix", GL_FLOAT_MAT4, update_perspective_matrix);
	rage->shader->update_uniforms(rage);
	rage->shader->init_uniform("u_model_matrix", GL_FLOAT_MAT4, update_model_matrix);

	rage->skybox_shader = new RAGE_shader((rage->executable_path + "/shaders/vertex_skybox.glsl").c_str(),
										  (rage->executable_path + "/shaders/fragment_skybox.glsl").c_str());
	rage->skybox_shader->init_uniform("u_view_matrix", GL_FLOAT_MAT4, update_view_matrix);
	rage->skybox_shader->init_uniform("u_perspective_matrix", GL_FLOAT_MAT4, update_perspective_matrix);
	rage->skybox_shader->init_uniform("u_skybox", GL_SAMPLER_CUBE, NULL);
	rage->skybox_shader->update_uniforms(rage);
}


RAGE_shader::~RAGE_shader()
{
	if (program != 0)
	{
		glDeleteProgram(program);
	}
}