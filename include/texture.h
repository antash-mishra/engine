#ifndef TEXTURE_H
#define TEXTURE_H

#include <glad/glad.h>
#include <string>

class Texture {
public:
    // Texture ID
    unsigned int ID;
    
    // Texture dimensions
    int width;
    int height;
    int channels;
    
    // Texture type
    GLenum type;
    GLenum format;
    
    // Default constructor
    Texture();
    
    // Constructor with path
    Texture(const std::string& path, bool flipVertically = false, GLenum type = GL_TEXTURE_2D);
    
    // Destructor
    ~Texture();
    
    // Bind texture to a specific texture unit
    void bind(unsigned int unit = 0) const;
    
    // Unbind texture
    void unbind() const;
    
    // Generate texture
    void generate(unsigned char* data);
    
    // Load texture from file
    bool loadFromFile(const std::string& path, bool flipVertically = false);
    
    // Set texture parameters
    void setParameter(GLenum paramName, GLint param);
    void setParameter(GLenum paramName, GLfloat param);
    
    // Get texture's internal format based on channels
    GLenum getInternalFormat() const;
    
    // Check if texture is loaded
    bool isLoaded() const;

private:
    // Delete the texture
    void cleanup();
    
    // Flag to indicate if the texture is loaded
    bool m_isLoaded = false;
};

#endif // TEXTURE_H 