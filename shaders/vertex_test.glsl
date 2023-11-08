#version 330 core

layout (location = 0) in vec3 position_attr;
layout (location = 1) in vec3 color_attr;

out vec3 color;

void main()
{
	gl_Position = vec4(position_attr.x, position_attr.y, position_attr.z, 1.0);
	color = color_attr;
}