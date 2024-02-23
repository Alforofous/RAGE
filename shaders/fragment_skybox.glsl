#version 330 core

in vec3 frag_position;

uniform samplerCube u_skybox;
uniform vec4 color;

out vec4 frag_color;

void main()
{
	vec4 texture_color = texture(u_skybox, frag_position);
	frag_color = texture_color;
}