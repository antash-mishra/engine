#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
// Per-instance model matrix occupies locations 3-6
layout (location = 3) in mat4 instanceModel;

uniform mat4 view;
uniform mat4 projection;

out vec2 TexCoords;
out vec3 Normal;
out vec3 WorldPos;

uniform bool reverse_normals;

void main()
{
    mat4 model = instanceModel;
    WorldPos = vec3(model * vec4(aPos, 1.0));
    Normal   = transpose(inverse(mat3(model))) * aNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
