#version 430 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gColorSpec;
uniform sampler2D ssao;

uniform vec3 lightColor;

void main() {
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal  = texture(gNormal, TexCoords).rgb;
    vec3 Albedo  = texture(gColorSpec, TexCoords).rgb;
    float ambientOcclusion = texture(ssao, TexCoords).r;
    
    // Simple ambient lighting modulated by albedo so the model's base color is visible
    vec3 ambientStrength = 0.3 * ambientOcclusion * Albedo;
    vec3 ambient = ambientStrength;
    vec3 gammaCorrected = pow(ambient, vec3(1.0 / 2.2));
    FragColor = vec4(ambient, 1.0);
}