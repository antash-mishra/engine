#version 330 core
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
    float Radius;
};

const int NR_LIGHTS = 32;
uniform Light lights[NR_LIGHTS];
uniform vec3 viewPos;

void main() {
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal  = texture(gNormal, TexCoords).rgb;
    vec3 Albedo  = texture(gColorSpec, TexCoords).rgb;
    float SpecularStrength = texture(gColorSpec, TexCoords).a;

    // Ambient
    vec3 lighting = Albedo * 0.1; // lowered ambient

    vec3 viewDir = normalize(viewPos - FragPos);
    for(int i = 0; i < NR_LIGHTS; ++i)
    {
        // light direction & distance
        vec3 lightDir = lights[i].Position - FragPos;
        float distance = length(lightDir);

        if (distance < lights[i].Radius) {
            lightDir = normalize(lightDir);
            
            // diffuse term
            float diff = max(dot(Normal, lightDir), 0.0);
            vec3 diffuse = diff * Albedo * lights[i].Color;

            // specular term (Blinn-Phong)
            vec3 halfwayDir = normalize(lightDir + viewDir);
            float spec = pow(max(dot(Normal, halfwayDir), 0.0), 16.0);
            vec3 specular = spec * SpecularStrength * lights[i].Color;

            // attenuation (quadratic fall-off)
            float attenuation = 1.0 / (1.0 + lights[i].Linear * distance + lights[i].Quadratic * distance * distance);

            diffuse *= attenuation;
            specular *= attenuation;
            lighting += diffuse + specular;
        }
    }

    // gamma correction for display
    vec3 gammaCorrected = pow(lighting, vec3(1.0/2.2));
    FragColor = vec4(gammaCorrected, 1.0);
}
