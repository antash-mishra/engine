#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords_0;
layout (location = 3) in vec2 aTexCoords_1;
layout (location = 4) in vec4 aTangent;

uniform mat4 MVP;
uniform mat4 model;
uniform mat4 view;

out vec2 TexCoords;
out vec3 FragPos;
out vec3 Normal;


void main() {
    vec4 viewPos = view * model * vec4(aPos, 1.0);
    TexCoords = aTexCoords_0;
    FragPos = viewPos.xyz;
    mat3 normalMatrix = mat3(transpose(inverse(view * model)));
    Normal = normalize(normalMatrix * aNormal);
    gl_Position = MVP * vec4(aPos, 1.0);
}