#include "RAGE_primitive_objects.hpp"

static GLfloat *get_cube_vertex_positions(float width, float height, float depth)
{
	GLfloat *vertex_positions = new GLfloat[CUBE_VERTEX_COUNT * 3]
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

RAGE_object *RAGE_primitive_objects::create_cube(float width, float height, float depth)
{
	GLfloat *vertex_positions = get_cube_vertex_positions(width, height, depth);
	if (vertex_positions == NULL)
		return (NULL);
	GLuint *indices = new GLuint[CUBE_TRIANGLE_COUNT * 3]
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

	RAGE_mesh *mesh = new RAGE_mesh();
	mesh->set_vertex_positions(vertex_positions, CUBE_VERTEX_COUNT * 3);
	mesh->set_indices(indices, CUBE_TRIANGLE_COUNT * 3);
	RAGE_object *object = new RAGE_object(mesh, "Cube");
	return (object);
}
