#include "RAGE.hpp"
#include "RAGE_shader.hpp"

RAGE_shader::RAGE_shader(const std::string& vertex_path, const std::string& fragment_path)
{
	this->vertex_path = vertex_path;
	this->fragment_path = fragment_path;
	std::string vertex_string = read_file(vertex_path);
	std::string fragment_string = read_file(fragment_path);
	create(vertex_string, fragment_string);
}

std::string RAGE_shader::read_file(const std::string& file_path)
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

GLuint RAGE_shader::compile(GLuint type, const std::string& source)
{
	GLuint shader = glCreateShader(type);

	const char* src = source.c_str();
	glShaderSource(shader, 1, &src, nullptr);

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

void RAGE_shader::create(const std::string& vertex_string, const std::string& fragment_string)
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
		std::cout << "Warning: variable_name is NULL" << std::endl;
		return (-1);
	}
	RAGE_uniform uniform;
	uniform.location = glGetUniformLocation(this->program, variable_name);
	if (uniform.location == -1)
		std::cout << "Warning: Failed to locate uniform variable " << variable_name << std::endl;
	this->uniforms[variable_name] = uniform;
	return (uniform.location);
}

GLint RAGE_shader::init_uniform(const char *variable_name, GLenum type, std::function<void(void *content)> set_value_per_frame)
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
	uniform.per_frame_callback = set_value_per_frame;
	this->uniforms[variable_name] = uniform;
	return (uniform.location);
}

void RAGE_shader::update_uniforms(void *content)
{
	RAGE *rage;

	rage = (RAGE *)content;
	glUseProgram(this->program);
	for (auto& [key, uniform] : this->uniforms)
	{
		if (uniform.per_frame_callback != NULL)
			uniform.per_frame_callback(&uniform.location);
	}
}

void RAGE_shader::init_RAGE_shaders(void *content)
{
	RAGE *rage;

	rage = (RAGE *)content;
	rage->shader = new RAGE_shader(rage->executable_path + "/shaders/vertex_test.glsl",
								   rage->executable_path + "/shaders/fragment_test.glsl");
	rage->shader->init_uniform("u_view_matrix", GL_FLOAT_MAT4, update_view_matrix);
	rage->shader->init_uniform("u_perspective_matrix", GL_FLOAT_MAT4, update_perspective_matrix);
	rage->shader->init_uniform("u_model_matrix", GL_FLOAT_MAT4, NULL);

	rage->skybox_shader = new RAGE_shader(rage->executable_path + "/shaders/vertex_skybox.glsl",
										  rage->executable_path + "/shaders/fragment_skybox.glsl");
	rage->skybox_shader->init_uniform("u_view_matrix", GL_FLOAT_MAT4, update_view_matrix);
	rage->skybox_shader->init_uniform("u_perspective_matrix", GL_FLOAT_MAT4, update_perspective_matrix);
	rage->skybox_shader->init_uniform("u_skybox", GL_SAMPLER_CUBE, NULL);

	GLint maxTextureUnits;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
	std::cout << "Max texture image units: " << maxTextureUnits << std::endl;

	GLint maxArrayTextureLayers;
	glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayTextureLayers);
	std::cout << "Max array texture layers: " << maxArrayTextureLayers << std::endl;
}


RAGE_shader::~RAGE_shader()
{
	if (program != 0)
	{
		glDeleteProgram(program);
	}
}