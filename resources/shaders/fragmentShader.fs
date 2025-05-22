#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D texture_diffuse0;
uniform sampler2D texture_specular0;

uniform vec3 viewPos;
uniform vec3 lightPos;
vec3 calcDirLight( vec3 normal, vec3 viewDir);

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // directional light
    vec3 result = calcDirLight(norm, viewDir);

    FragColor = vec4(result, 1.0);
}


vec3 calcDirLight( vec3 normal, vec3 viewDir) {
    vec3 lightDirection = vec3(-0.2, -1.0, -0.3);
    vec3 lightDir = normalize(-lightDirection);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);

    // result
    vec3 ambient = vec3(0.2, 0.2, 0.2) * texture(texture_diffuse0, TexCoords).rgb;
    vec3 diffuse = vec3(0.5, 0.5, 0.5) * diff * texture(texture_diffuse0, TexCoords).rgb;
    vec3 specular = vec3(1.0, 1.0, 1.0) * spec * texture(texture_specular0, TexCoords).rgb;

    return (ambient + diffuse + specular);
}

