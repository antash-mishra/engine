#version 430 core

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

void main()
{
    // Use the y component of FragPos for grayscale
    float gray = FragPos.y;
    // Optionally normalize gray to [0,1] (if you know the min/max height, you can scale it)
    // For now, just clamp to [0,1] for safety
    gray = clamp(gray / 64.0 + 0.25, 0.0, 1.0); // Adjust scaling/offset as needed
    FragColor = vec4(gray, gray, gray, 1.0);
}