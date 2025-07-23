#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class Shader {
  public:
    unsigned int ID;

    Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath = nullptr,
      const char* tessControlPath=nullptr, const char* tessEvalPath=nullptr);
    Shader(const char* computePath);

    void use();

    void setBool(const std::string &name, bool value) const;
    void setInt(const std::string &name, int value) const;
    void setFloat(const std::string &name, float value) const;
    // For vec2 and uvec2
    // Note: glm::uvec2 is used for unsigned integer vectors
    void setVec2(const std::string &name, const  glm::vec2 &value) const;
    void setVec2(const std::string &name, float x, float y) const;
    void setUvec2(const std::string &name, const glm::uvec2 &value) const;
    void setUvec2(const std::string &name, unsigned int x, unsigned int y) const;
    // For vec3 and uvec3
    void setVec3(const std::string &name, const glm::vec3 &value) const;
    void setVec3(const std::string &name, float x, float y, float z) const;
    void setUvec3(const std::string &name, const glm::uvec3 &value) const;
    void setUvec3(const std::string &name, unsigned int x, unsigned int y, unsigned int z) const;
    // For vec4
    void setVec4(const std::string &name, const glm::vec4 &value) const;
    void setVec4(const std::string &name, float x, float y, float z, float w) const;
    // For mat2, mat3, and mat4
    // Note: glm::mat2, glm::mat3, and glm::mat4 are used for 2x2, 3x3, and 4x4 matrices respectively
    // These functions set the uniform matrix in the shader program
    void setMat2(const std::string &name, const glm::mat2 &mat) const;
    void setMat3(const std::string &name, const glm::mat3 &mat) const;
    void setMat4(const std::string &name, const glm::mat4 &mat) const;
  private:
    // utility function for checking shader compilation/linking errors.
    // ------------------------------------------------------------------------
    void checkCompileErrors(GLuint shader, std::string type);

};

#endif
