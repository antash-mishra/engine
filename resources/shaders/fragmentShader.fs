#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

struct Light {
    vec3 Position;
    vec3 Color;
};

uniform Light lights[16];
uniform sampler2D texture_diffuse0;
uniform vec3 viewPos;


void main()
{
    vec3 color = texture(texture_diffuse0, TexCoords).rgb;
    vec3 normal = normalize(Normal);
    vec3 lightColor = vec3(0.3);
    // ambient
    vec3 ambient = 0.0 * color;
    
    // lighting
    vec3 lighting = vec3(0.0);

    for(int i = 0; i < 16; i++)
    {
        // diffuse
        vec3 lightDir = normalize(lights[i].Position - FragPos);
        float diff = max(dot(lightDir, normal), 0.0);
        vec3 diffuse = lights[i].Color * diff * color;
        vec3 result = diffuse;

        // attenuation (use quadratic as we have gamma correction)
        float distance = length(FragPos - lights[i].Position);
        result *= 1.0 / (distance * distance);
        lighting += result;
    }

    vec3 lightingResult = ambient + lighting;
    // // also gamma correct while we're at it
    // lightingResult = pow(lightingResult, vec3(1.0 / 2.2));
    FragColor = vec4(lightingResult, 1.0);
}