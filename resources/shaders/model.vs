#version 430 core
layout (location = 0) in vec3 aPos;   
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool invertedNormals;

out vec2 TexCoords;
out vec3 FragPos;
out vec3 Normal;

void main()
{
    vec4 viewPos = view * model * vec4(aPos, 1.0f);
    TexCoords = aTexCoords;
    FragPos = viewPos.xyz;

    Normal = transpose(inverse(mat3(view * model))) * (invertedNormals ? -aNormal : aNormal);
    gl_Position = projection * viewPos;
}  