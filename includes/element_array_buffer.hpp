#include<glad/glad.h>

class element_array_buffer
{
public:
	GLuint	id;
	element_array_buffer(GLuint *indices, GLsizeiptr size);

	void	bind();
	void	unbind();
	void	delete_object();
};