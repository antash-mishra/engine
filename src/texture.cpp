#include "texture.h"
#include "stb_image.h"
#include <iostream>

Texture::Texture() 
    : ID(0), width(0), height(0), channels(0), type(GL_TEXTURE_2D), format(GL_RGB), m_isLoaded(false)
{
}

Texture::Texture(const std::string& path, bool flipVertically, GLenum type)
    : ID(0), width(0), height(0), channels(0), type(type), format(GL_RGB), m_isLoaded(false)
{
    loadFromFile(path, flipVertically);
}

Texture::~Texture() {
    cleanup();
}

void Texture::bind(unsigned int unit) const {
    if (!m_isLoaded) {
        std::cout << "WARNING: Trying to bind a texture that isn't loaded!" << std::endl;
        return;
    }
    
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(type, ID);
}

void Texture::unbind() const {
    glBindTexture(type, 0);
}

void Texture::generate(unsigned char* data) {
    if (ID != 0) {
        cleanup(); // Delete existing texture if any
    }
    
    glGenTextures(1, &ID);
    glBindTexture(type, ID);
    
    // Set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(type, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(type, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    if (data) {
        GLenum internalFormat = getInternalFormat();
        glTexImage2D(type, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(type);
        m_isLoaded = true;
    } else {
        std::cout << "ERROR: Failed to generate texture, data is null" << std::endl;
        m_isLoaded = false;
    }
    
    glBindTexture(type, 0); // Unbind
}

bool Texture::loadFromFile(const std::string& path, bool flipVertically) {
    // Clear any existing texture
    cleanup();
    
    // Load image using stb_image
    stbi_set_flip_vertically_on_load(flipVertically);
    
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    
    if (data) {
        // Set format based on channels
        if (channels == 1)
            format = GL_RED;
        else if (channels == 3)
            format = GL_RGB;
        else if (channels == 4)
            format = GL_RGBA;
        
        // Generate texture
        generate(data);
        
        // Free image data
        stbi_image_free(data);
        
        std::cout << "Texture loaded: " << path << " (" << width << "x" << height << ", " << channels << " channels)" << std::endl;
        return true;
    } else {
        std::cout << "ERROR: Failed to load texture: " << path << std::endl;
        std::cout << "stbi_failure_reason: " << stbi_failure_reason() << std::endl;
        return false;
    }
}

void Texture::setParameter(GLenum paramName, GLint param) {
    glBindTexture(type, ID);
    glTexParameteri(type, paramName, param);
    glBindTexture(type, 0);
}

void Texture::setParameter(GLenum paramName, GLfloat param) {
    glBindTexture(type, ID);
    glTexParameterf(type, paramName, param);
    glBindTexture(type, 0);
}

GLenum Texture::getInternalFormat() const {
    switch (channels) {
        case 1: return GL_RED;
        case 3: return GL_RGB;
        case 4: return GL_RGBA;
        default: return GL_RGB;
    }
}

bool Texture::isLoaded() const {
    return m_isLoaded;
}

void Texture::cleanup() {
    if (ID != 0) {
        glDeleteTextures(1, &ID);
        ID = 0;
    }
    
    width = 0;
    height = 0;
    channels = 0;
    m_isLoaded = false;
} 