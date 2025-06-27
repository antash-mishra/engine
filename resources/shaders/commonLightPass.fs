#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gColorSpec;

struct Light {
    vec3 Position;
    vec3 Color;
    
    float Linear;
    float Quadratic;
};

uniform Light light;
uniform vec3 viewPos;
uniform vec2 screenSize;

void main() {
    vec2 uv = TexCoords;
    if (uv == vec2(0.0)) {
        uv = gl_FragCoord.xy / screenSize;
    }
    vec3 FragPos = texture(gPosition, uv).rgb;
    vec3 Normal  = texture(gNormal, uv).rgb;
    vec3 Albedo  = texture(gColorSpec, uv).rgb;
    float SpecularStrength = texture(gColorSpec, uv).a;


    // Calculate lighting for this single point light
    vec3 lightDir = light.Position - FragPos;
    float distance = length(lightDir);
    lightDir = normalize(lightDir);
    
    // Diffuse term
    float diff = max(dot(Normal, lightDir), 0.0);
    vec3 diffuse = diff * Albedo * light.Color;

    // Specular term (Blinn-Phong)
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(Normal, halfwayDir), 0.0), 16.0);
    vec3 specular = spec * SpecularStrength * light.Color;

    // Attenuation (quadratic fall-off)
    float attenuation = 1.0 / (1.0 + light.Linear * distance + light.Quadratic * distance * distance);

    diffuse *= attenuation;
    specular *= attenuation;
    
    vec3 lighting = diffuse + specular;
    // Gamma correction (clamp to avoid NaN)
    lighting = max(lighting, vec3(0.0));
    vec3 gammaCorrected = pow(lighting, vec3(1.0/2.2));

    FragColor = vec4(lighting, 1.0);
}