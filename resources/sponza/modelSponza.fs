#version 330 core
layout (location = 0) out vec4 FragColor;


in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D baseColorTexture;

void main()
{    
    FragColor = texture(baseColorTexture, TexCoords);
}