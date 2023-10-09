
#define GLFW_INCLUDE_NONE
#include "RAGE.hpp"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include <iostream>
#include "ShaderLoader.hpp"

static void error_callback(int error, const char *description)
{
	fprintf(stderr, "Error: %s\n", description);
}

int main(void)
{
	RAGE	*rage = new RAGE();
	if (glfwInit() == GLFW_FALSE)
		return (1);
	if (rage->window->Init() == -1)
		return (1);

	/*Load GL*/
	glfwSetErrorCallback(error_callback);
	glfwMakeContextCurrent(rage->window->glfw_window);
	gladLoadGL();

	/*Load shaders*/
	ShaderLoader shaderLoader("../shaders/vertex_test.glsl", "../shaders/fragment_test.glsl");

	/*Load viewport and start using shader program*/
	glViewport(0, 0, rage->window->mode->width, rage->window->mode->height);
	glUseProgram(shaderLoader.hProgram);

	/*Set GLSL variable locations*/
	int resolutionLocation = glGetUniformLocation(shaderLoader.hProgram, "u_resolution");

	int width;
	int height;
	while (!glfwWindowShouldClose(rage->window->glfw_window))
	{
		glfwGetWindowSize(rage->window->glfw_window, &width, &height);
		glUniform2f(resolutionLocation, (float)width, (float)height);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glfwPollEvents();
		glfwSwapBuffers(rage->window->glfw_window);
	}
	delete rage;
	glfwTerminate();
	return (0);
}

/*

#define GLFW_INCLUDE_NONE
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <stdlib.h>
#include <stdio.h>

static const struct
{
	float x, y;
	float r, g, b;
} vertices[3] =
{
	{ -0.6f, -0.4f, 1.f, 0.f, 0.f },
	{  1.6f, -0.4f, 0.f, 1.f, 0.f },
	{  0.0f,  0.6f, 0.f, 0.f, 1.f }
};

static void error_callback(int error, const char *description)
{
	fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

#include "glm/ext.hpp"
#include "ShaderLoader.hpp"

int main(void)
{
	GLFWwindow *window;
	GLuint vertex_buffer;
	GLint mvp_location, vpos_location, vcol_location;

	glfwSetErrorCallback(error_callback);

	if (!glfwInit())
		exit(EXIT_FAILURE);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	window = glfwCreateWindow(640, 480, "Simple example", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwSetKeyCallback(window, key_callback);

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	gladLoadGL();

	// NOTE: OpenGL error checks have been omitted for brevity

	ShaderLoader shaderLoader("../shaders/vertex_test.glsl", "../shaders/fragment_test.glsl");

	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	mvp_location = glGetUniformLocation(shaderLoader.hProgram, "MVP");
	vpos_location = glGetAttribLocation(shaderLoader.hProgram, "vPos");
	vcol_location = glGetAttribLocation(shaderLoader.hProgram, "vCol");
	GLint resolution = glGetAttribLocation(shaderLoader.hProgram, "u_resolution");
	glUseProgram(shaderLoader.hProgram);

	glEnableVertexAttribArray(vpos_location);
	glVertexAttribPointer(vpos_location, 2, GL_FLOAT, GL_FALSE,
		sizeof(vertices[0]), (void *)0);
	glEnableVertexAttribArray(vcol_location);
	glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE,
		sizeof(vertices[0]), (void *)(sizeof(float) * 2));

	int	width;
	int	height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);
	float ratio = width / (float)height;
	glm::mat4 projection_matrix = glm::ortho(-ratio, ratio, -1.0f, 1.0f, 1.0f, -1.0f);
	while (!glfwWindowShouldClose(window))
	{
		glUniform2f(resolution, (float)width, (float)height);
		glm::mat4 matrix(1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		matrix = glm::rotate(matrix, (float)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.3f));
		glm::mat4 product_matrix = matrix * projection_matrix;

		glUniformMatrix4fv(mvp_location, 1, GL_FALSE, glm::value_ptr(product_matrix));
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwDestroyWindow(window);

	glfwTerminate();
	exit(EXIT_SUCCESS);
}
*/
