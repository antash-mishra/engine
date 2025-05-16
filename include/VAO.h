#ifndef VAO_H
#define VAO_H

#include <glad/glad.h>
#include "VBO.h"

class VAO {
  public:
    GLuint ID;
    VAO();

	  // Links a VBO Attribute such as a position or color to the VAO
	  void LinkAttrib(VBO& VBO, GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, void* offset);

	  // Binds the VAO
    void Bind();

	  // Unbinds the VAO
    void Unbind();

	  // Deletes the VAO
    void Delete();
};

#endif