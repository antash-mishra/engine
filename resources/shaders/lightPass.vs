#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool useQuadRendering;

out vec2 TexCoords;

void main() {
    if (useQuadRendering) {
        TexCoords = aTexCoords;
        gl_Position = vec4(aPos, 1.0);
    } else {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        // TexCoords will be computed in fragment using gl_FragCoord
        TexCoords = vec2(0.0);
    }
}