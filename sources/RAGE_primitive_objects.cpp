#include "RAGE_primitive_objects.hpp"
#include "loaders/GLB_attribute_buffer.hpp"

static GLfloat *get_cube_vertex_positions(float width, float height, float depth)
{
	GLfloat *vertex_positions = new (std::nothrow) GLfloat[CUBE_VERTEX_COUNT * 3]
	{
		-width / 2.0f, -height / 2.0f, -depth / 2.0f,
		width / 2.0f, -height / 2.0f, -depth / 2.0f,
		width / 2.0f, height / 2.0f, -depth / 2.0f,
		-width / 2.0f, height / 2.0f, -depth / 2.0f,
		-width / 2.0f, -height / 2.0f, depth / 2.0f,
		width / 2.0f, -height / 2.0f, depth / 2.0f,
		width / 2.0f, height / 2.0f, depth / 2.0f,
		-width / 2.0f, height / 2.0f, depth / 2.0f
	};
	if (vertex_positions == NULL)
		return (NULL);
	return (vertex_positions);
}

static GLfloat *get_cube_vertex_colors()
{
	GLuint color_channel_count = 4;
	GLfloat *vertex_colors = new (std::nothrow) GLfloat[CUBE_VERTEX_COUNT * color_channel_count];
	if (vertex_colors == NULL)
		return (NULL);
	std::fill_n(vertex_colors, CUBE_VERTEX_COUNT * color_channel_count, 0.7f);
	for (GLuint i = 0; i < CUBE_VERTEX_COUNT; i++)
	{
		vertex_colors[i * color_channel_count + 3] = 1.0f;
	}
	return (vertex_colors);
}

RAGE_primitive *RAGE_primitive_objects::create_cube(float width, float height, float depth)
{
	GLfloat *vertex_positions = get_cube_vertex_positions(width, height, depth);
	if (vertex_positions == NULL)
		return (NULL);
	GLfloat *vertex_colors = get_cube_vertex_colors();
	if (vertex_colors == NULL)
		return (NULL);
	GLuint *indices = new (std::nothrow) GLuint[CUBE_TRIANGLE_COUNT * 3]
	{
		0, 1, 2,	0, 2, 3,
		1, 5, 6,	1, 6, 2,
		5, 4, 7,	5, 7, 6,
		4, 0, 3,	4, 3, 7,
		3, 2, 6,	3, 6, 7,
		4, 5, 1,	4, 1, 0
	};
	if (indices == NULL)
		return (NULL);

	GLB_attribute_buffer *position_attribute_buffer = new (std::nothrow) GLB_attribute_buffer(vertex_positions, 0, 0, CUBE_VERTEX_COUNT, "POSITION", 3, GL_FLOAT, GL_FALSE);
	if (position_attribute_buffer == NULL)
		return (NULL);
	GLB_attribute_buffer *color_attribute_buffer = new (std::nothrow) GLB_attribute_buffer(vertex_colors, 0, 0, CUBE_VERTEX_COUNT, "COLOR_0", 4, GL_FLOAT, GL_FALSE);
	if (color_attribute_buffer == NULL)
		return (NULL);
	RAGE_primitive *primitive = new RAGE_primitive();
	primitive->attribute_buffers.push_back(position_attribute_buffer);
	primitive->attribute_buffers.push_back(color_attribute_buffer);
	primitive->element_buffer_object = new (std::nothrow) buffer_object(GL_ELEMENT_ARRAY_BUFFER, GL_UNSIGNED_INT, indices, CUBE_TRIANGLE_COUNT * 3 * sizeof(GLuint));
	primitive->indices_count = CUBE_TRIANGLE_COUNT * 3;
	if (primitive->element_buffer_object == NULL)
		return (NULL);
	if (primitive->interleave_vbos() == false)
		return (NULL);
	return (primitive);
}
