#version 330 core

layout (location = 0) in vec3 a_position;
layout (location = 3) in vec4 a_color;

uniform mat4 u_perspective_matrix;
uniform mat4 u_view_matrix;
uniform mat4 u_model_matrix;

out vec4 color;

void main()
{
	color = a_color;
	vec4 position = vec4(a_position.x, a_position.y, a_position.z, 1.0);
	gl_Position = u_perspective_matrix * u_view_matrix * u_model_matrix * position;
}