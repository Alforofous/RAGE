#include "RAGE.hpp"

RAGE_shader::RAGE_shader(const std::string& filePathVertexShader, const std::string& filePathFragmentShader)
{
	std::string vertexShader = ReadFile(filePathVertexShader);
	std::string fragmentShader = ReadFile(filePathFragmentShader);
	CreateShader(vertexShader, fragmentShader);
	glUseProgram(hProgram);
}

RAGE_shader::~RAGE_shader()
{
	if (hProgram != 0)
	{
		glDeleteProgram(hProgram);
	}
}

std::string RAGE_shader::ReadFile(const std::string& filePath)
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

GLuint RAGE_shader::CompileShader(GLuint type, const std::string& source)
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

void RAGE_shader::CreateShader(const std::string& vertexShader, const std::string& fragmentShader)
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

void RAGE_shader::init_variable_locations()
{
	const char	*variable_names[] =
	{
		"u_perspective_matrix",
		"u_view_matrix",
		"u_model_matrix",
	};
	int variable_count = sizeof(variable_names) / sizeof(variable_names[0]);
	for (int i = 0; i < variable_count; i++)
	{
		variable_location[variable_names[i]] = glGetUniformLocation(hProgram, variable_names[i]);
		if (variable_location[variable_names[i]] == -1)
		{
			std::cout << "Warning: Failed to locate uniform variable " << variable_names[i] << std::endl;
		}
	}
}

void set_shader_variable_values(void *content)
{
	int width;
	int height;
	RAGE *rage;
	RAGE_camera *camera;
	std::map<std::string, GLint> locations;

	rage = (RAGE *)content;
	camera = &rage->camera;
	locations = rage->shader->variable_location;
	glfwGetWindowSize(rage->window->glfw_window, &width, &height);
	glUniform2f(locations["u_resolution"], (float)width, (float)height);
	glUniformMatrix4fv(locations["u_perspective_matrix"], 1, GL_FALSE, glm::value_ptr(camera->get_perspective_matrix()));
	glUniformMatrix4fv(locations["u_view_matrix"], 1, GL_FALSE, glm::value_ptr(camera->get_view_matrix()));
}