#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gColorSpec;

uniform vec3 lightColor;

void main() {
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal  = texture(gNormal, TexCoords).rgb;
    vec3 Albedo  = texture(gColorSpec, TexCoords).rgb;
    
    // Simple ambient lighting modulated by albedo so the model's base color is visible
    vec3 ambientStrength = 0.5 * lightColor;
    vec3 ambient = ambientStrength * Albedo;
    vec3 gammaCorrected = pow(ambient, vec3(1.0 / 2.2));
    FragColor = vec4(ambient, 1.0);
}