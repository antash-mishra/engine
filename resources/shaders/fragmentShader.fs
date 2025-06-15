#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D texture_diffuse0;
// uniform sampler2D texture_specular0;

uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];
uniform vec3 viewPos;


vec3 blinnPhong(vec3 lightPos, vec3 lightColor, vec3 normal, vec3 viewDir, vec3 fragPos)
{
    // diffuse
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * lightColor;

    // specular
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular = spec * lightColor;

    return diffuse + specular;
}

void main()
{
    vec3 color = texture(texture_diffuse0, TexCoords).rgb;
    
    vec3 lighting == vec3(0.0);
    
    // diffuse
    for (int i=0; i<4; i++) {
        vec3 lightPos = lightPositions[i];
        vec3 lightColor = lightColors[i];
        lighting += blinnPhong(lightPos, lightColor, normalize(Normal), normalize(viewPos - FragPos), FragPos);
    }

    color *= lighting;

    // Apply gamma correction
    color = pow(color, vec3(1.0 / 2.2));
    // Final color output
    FragColor = vec4(color, 1.0);
}