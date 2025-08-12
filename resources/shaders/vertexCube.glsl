#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;

out vec2 TexCoords;
//out vec3 Normal;
out vec3 FragPos;

uniform bool reverse_normals;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
//    Normal = transpose(inverse(mat3(model))) * aNormal;
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos, 1.0);
}
