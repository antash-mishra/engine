// #version 330 core
// out vec4 FragColor;

// in vec2 texCoord;
// in vec3 Normal;
// in vec3 FragPos;

// uniform sampler2D diffuse0;
// uniform sampler2D specular0;

// uniform vec3 lightPos;
// uniform vec3 lightColor;
// uniform vec3 viewPos;

// vec3 calcDirLight(vec3 normal, vec3 viewDir);
// vec3 calcPointLight(vec3 normal, vec3 fragPos, vec3 viewDir);
// vec3 calcFlashLight(vec3 normal, vec3 fragPos, vec3 viewDir);



// void main()
// {
    
//     vec3 norm = normalize(Normal);
//     vec3 viewDir = normalize(viewPos - FragPos);
    
//     vec3 result = calcPointLight(norm, FragPos, viewDir);
//     vec3 flashLight = calcFlashLight(norm, FragPos, viewDir);
//     FragColor = vec4(flashLight, 1.0);
// }


// vec3 calcPointLight(vec3 normal, vec3 fragPos, vec3 viewDir) {
//     vec3 lightDir = normalize(lightPos - fragPos);

//         // diffuse shading
//     float diff = max(dot(normal, lightDir), 0.0);

//     // specular shading
//     vec3 reflectDir = reflect(-lightDir, normal);
//     float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);

//     // attenuation
//     float distance = length(lightPos - fragPos);
//     float attenuation = 1.0/(1.0 +  0.7 * distance + 3.0 * (distance * distance));

//     vec3 ambient = lightColor * texture(diffuse0, texCoord).rgb;
//     vec3 diffuse = lightColor * diff * texture(diffuse0, texCoord).rgb;
//     vec3 specular = lightColor * spec * texture(specular0, texCoord).rgb;

//     ambient *= attenuation;
//     diffuse *= attenuation;
//     specular *= attenuation;

//     return (ambient + diffuse + specular);
// }


// vec3 calcFlashLight(vec3 normal, vec3 fragPos, vec3 viewDir) {
// 	// controls how big the area that is lit up is
// 	float outerCone = 0.90f;
// 	float innerCone = 0.95f;

// 	// ambient lighting
// 	float ambient = 0.20f;

// 	// diffuse lighting
// 	vec3 lightDirection = normalize(lightPos - fragPos);
// 	float diffuse = max(dot(normal, lightDirection), 0.0f);

// 	// specular lighting
//     float specularLight = 0.50f;
// 	vec3 reflectionDirection = reflect(-lightDirection, normal);
// 	float specular = pow(max(dot(viewDir, reflectionDirection), 0.0f), 16);
    
//     // calculate intensity of the spotlight
// 	float angle = dot(vec3(0.0f, -1.0f, 0.0f), -lightDirection);
// 	float intensity = clamp((angle - outerCone) / (innerCone - outerCone), 0.0f, 1.0f);

//     return (texture(diffuse0, texCoord) * (diffuse * intensity + ambient) + texture(specular0, texCoord).r * specular * intensity) * lightColor;
    
// }


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
    vec3 ambient = vec3(0.1, 0.1, 0.1) * texture(texture_diffuse0, TexCoords).rgb;
    vec3 diffuse = vec3(0.8, 0.8, 0.8) * diff * texture(texture_diffuse0, TexCoords).rgb;
    vec3 specular = vec3(1.0, 1.0, 1.0) * spec * texture(texture_specular0, TexCoords).rgb;

    return (ambient + diffuse + specular);
}

