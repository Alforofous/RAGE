#include "RAGE.hpp"

void update_view_matrix(GLint location, void *content)
{
	RAGE *rage = (RAGE *)content;
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(rage->camera.get_view_matrix()));
}

void update_perspective_matrix(GLint location, void *content)
{
	RAGE *rage = (RAGE *)content;
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(rage->camera.get_perspective_matrix()));
}

void update_model_matrix(GLint location, void *content)
{
	RAGE_object *object = (RAGE_object *)content;
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(object->get_model_matrix()));
}