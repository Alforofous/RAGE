#version 330 core

in vec3 frag_position;

uniform samplerCube u_skybox;
uniform vec4 color;

out vec4 fragColor;

void main()
{
	vec4 texColor = texture(u_skybox, frag_position);
	fragColor = texColor * color;
}