#version 330 core

layout (location = 0) in vec3 a_position;

uniform mat4 u_perspective_matrix;
uniform mat4 u_view_matrix;

out vec3 frag_position;

void main()
{
	mat4 view = mat4(mat3(u_view_matrix));
	frag_position = a_position;
	gl_Position = u_perspective_matrix * view * vec4(a_position * 1000.0, 1.0);
}