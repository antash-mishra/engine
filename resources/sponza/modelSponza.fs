#version 330 core
layout (location = 0) out vec4 FragColor;

#define NR_LIGHTS 1
#define DIR_LIGHT_COUNT 1

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;

struct Light {
    vec3 direction; // For directional lights: direction vector (points FROM light)
    vec3 color;
    float intensity;
    vec3 lightPos; // especially for point lights
    bool isPointLight;
};

struct DirLight {
    vec3 direction; // For directional lights: direction vector (points FROM light)
    vec3 color;
    float intensity;
    vec3 lightPos; // especially for point lights
    bool isPointLight;
};

uniform sampler2D baseColorTexture;
uniform sampler2D normalTexture;
uniform sampler2D metallicRoughnessTexture;
uniform float metallicFactor;

uniform sampler2D shadowMap;

uniform Light light[NR_LIGHTS];
uniform DirLight dirLight[DIR_LIGHT_COUNT];
uniform vec3 viewPos;


float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // check whether current frag pos is in shadow
    float bias = max(0.3 * (1.0 - dot(normal, lightDir)), 0.03);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    if(projCoords.z > 1.0)
        shadow = 0.0;
    return shadow;
}



// Simple Blinn-Phong point-light calculation with quadratic attenuation
vec3 calcPointLight(Light lgt, vec3 normal, vec3 fragPos, vec3 viewDir) {
    // Direction from fragment toward the light
    vec3 lightDir = normalize(lgt.lightPos - fragPos);

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);

    // Specular
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    // Quadratic attenuation ( constant = 1, linear = 0, quad = 1/(distance^2) )
    float distance = length(lgt.lightPos - fragPos);
    float attenuation = 1.0 / (distance * distance);

    
//     vec3 ambient  = 0.05 * lgt.color;
    vec3 diffuse  = diff * lgt.color;
    vec3 specular = spec * lgt.color;

//     ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;

    return (diffuse + specular) * lgt.intensity;
}


void main()
{
    vec3 color = texture(baseColorTexture, TexCoords).rgb;
    vec3 specColor = texture(metallicRoughnessTexture, TexCoords).rgb * metallicFactor;
    
    // Use normal map if available, otherwise use vertex normal
    vec3 normal = normalize(Normal);
    
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lighting = vec3(0.0f);
    // Directional light branch

    for (int i = 0; i < NR_LIGHTS; i++) {
        // Skip inactive lights
        // if (light[i].intensity <= 0.0) continue;
        lighting += calcPointLight(light[i], normal, FragPos, viewDir);
    }

    for (int j=0; j<DIR_LIGHT_COUNT; j++) {
        // Directional light calculations
        vec3 ambient = 0.05 * vec3(0.9999999, 0.9999998, 1.0) * dirLight[j].color;
        vec3 lightDir = normalize(-dirLight[j].direction); // Negate because direction points FROM light
        float diff = max(dot(lightDir, normal), 0.0);
        vec3 diffuse = dirLight[j].color * diff;

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
        vec3 specular = dirLight[j].color * spec * specColor;
        float shadow = ShadowCalculation(FragPosLightSpace, normal, lightDir);
        lighting += (ambient + (1.0 - shadow) * (diffuse + specular)) * 0.4;
    }

    color *= lighting;

    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, 1.0);
}