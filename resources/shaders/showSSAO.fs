#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D ssaoTex;

void main() {
    float occlusion = texture(ssaoTex, TexCoords).r;
    FragColor = vec4(vec3(occlusion), 1.0);
} 