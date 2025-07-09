#version 330 core
layout (location = 0) in vec3 aPos;   
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 MVP;

out vec2 TexCoords;
out vec3 FragPos;
out vec3 Normal;

void main()
{
    TexCoords = aTexCoords;
    FragPos = vec3(MVP * vec4(aPos, 1.0f));
    Normal = aNormal;
    gl_Position = MVP * vec4(aPos, 1.0f);
}  