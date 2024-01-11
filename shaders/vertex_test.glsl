#version 330 core

layout (location = 0) in vec3 position_attr;
layout (location = 3) in vec4 color_attr;

uniform vec2 u_resolution;
uniform mat4 u_perspective_matrix;
uniform mat4 u_view_matrix;
uniform mat4 u_model_matrix;

out vec4 color;

void main()
{
	color = color_attr;
	vec4 position = vec4(position_attr.x, position_attr.y, position_attr.z, 1.0);
	gl_Position = u_perspective_matrix * u_view_matrix * u_model_matrix * position;
}