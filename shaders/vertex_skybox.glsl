#version 330 core

layout (location = 0) in vec3 position_attr;

uniform mat4 u_view_matrix;
uniform mat4 u_perspective_matrix;

out vec3 frag_position;

void main()
{
	mat4 view = mat4(mat3(u_view_matrix));
	frag_position = position_attr;
	gl_Position = u_perspective_matrix * view * vec4(position_attr, 1.0);
}