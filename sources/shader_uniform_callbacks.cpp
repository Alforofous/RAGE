#include "RAGE.hpp"

void update_view_matrix(void *content)
{
	RAGE *rage = get_rage();

	GLint location = *(GLint *)content;
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(rage->camera.get_view_matrix()));
}

void update_perspective_matrix(void *content)
{
	RAGE *rage = get_rage();

	GLint location = *(GLint *)content;
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(rage->camera.get_perspective_matrix()));
}