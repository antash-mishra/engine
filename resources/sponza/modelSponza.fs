#version 330 core
layout (location = 0) out vec4 FragColor;

#define NR_LIGHTS 1
#define DIR_LIGHT_COUNT 1
#define PI 3.14159265359

in vec2 TexCoords;
in vec2 TexCoords1;
in vec3 WorldPos;
in vec3 Normal;
in vec4 FragPosLightSpace;
in vec3 Tangent;
in vec3 Bitangent;

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

uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicRoughnessMap;
uniform samplerCube irradianceMap;
uniform sampler2D brdfLUT;
uniform samplerCube prefilterMap;
uniform sampler2D ssaoMap;
uniform float metallicFactor;
uniform float roughnessFactor;
uniform vec4 baseColorFactor;
uniform float exposure;
uniform float ambientScale;

// ADD_UNIFORM_AO
uniform sampler2D aoMap;
// END_ADD_UNIFORM_AO

uniform sampler2D shadowMap;

uniform Light light[NR_LIGHTS];
uniform DirLight dirLight[DIR_LIGHT_COUNT];
uniform vec3 viewPos;
uniform vec3 camPosition;



vec3 getNormalFromNormalMap()
{
    // Sample the normal map and convert from [0,1] to [-1,1] range
    vec3 tangentNormal = texture(normalMap, TexCoords1).xyz * 2.0 - 1.0;

    // Use the pre-calculated tangent space vectors from vertex shader
    // Note: Re-normalizing because interpolation can change vector lengths
    vec3 N = normalize(Normal);
    vec3 T = normalize(Tangent);
    vec3 B = normalize(Bitangent);

    // Construct the TBN matrix to transform from tangent space to world space
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0-roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float nDotH = max(dot(N, H) , 0.0);
    float nDotH2 = nDotH * nDotH;
    float num =  a2;
    float den = (nDotH2 * (a2 - 1)) + 1;
    den = PI * den * den;

    return num/den;
}

float GeometrySchlick(vec3 N, vec3 V, float roughness) {
    float nDotV = max(dot(N,V), 0.0);
    float r = (roughness +  1.0);
    float k = r*r / 8;

    float num = nDotV;
    float den = nDotV * (1.0 - k) + k;
    return num/den;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float geoL = GeometrySchlick(N, L, roughness);
    float geoV = GeometrySchlick(N, V, roughness);

    return geoL * geoV;
}

vec3 cookTorrence (vec3 N, vec3 V, vec3 L, float normalDis, float geoDis, vec3 fresnelDis) {
    vec3 num = normalDis * geoDis * fresnelDis;
    float den = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    return num/den;
}


float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // Early exit if outside shadow map
    if(projCoords.z > 1.0) return 0.0;
    
    // Get current depth
    float currentDepth = projCoords.z;
    
    // Calculate bias - smaller values for Sponza
    float bias = max(0.01 * (1.0 - dot(normal, lightDir)), 0.001);
    
    // Optional: Add slope-scale bias for better quality
    // bias += 0.0001 * tan(acos(dot(normal, lightDir)));
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    // 3x3 PCF
    for(int x = -2; x <= 2; ++x)
    {
        for(int y = -2; y <= 2; ++y)
        {
        float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
        shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 25.0;
    
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

vec3 tonemapACES(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x*(a*x+b)) / (x*(c*x+d)+e), 0.0, 1.0);
}


void main()
{
    // Apply gamma correction to convert from sRGB to linear space
    vec4 baseColor  = texture(albedoMap, TexCoords) * baseColorFactor;
    vec3 albedo     = baseColor.rgb;
    float metallic  = texture(metallicRoughnessMap, TexCoords).b * metallicFactor;  // Fixed: metallic is in blue channel, not red
    // glTF stores perceptual roughness (linear in perception). Convert to microfacet roughness (alpha)
    float roughness = texture(metallicRoughnessMap, TexCoords).g * roughnessFactor;
    // float roughness = perceptualRoughness * perceptualRoughness; // perceptual -> physical
    float ao        = texture(aoMap, TexCoords).r;
    float ssaoFactor = texture(ssaoMap, TexCoords).r;
    // Apply material factors

    // Use normal map if available, otherwise use vertex normal
    vec3 N = getNormalFromNormalMap();

    // viewPos
    vec3 V = normalize(camPosition - WorldPos);
    vec3 Lo = vec3(0.0);


     // base reflectivity of surface
     vec3 F0 = vec3(0.04);
     F0 = mix(F0, albedo, metallic);

    // for (int i = 0; i < NR_LIGHTS; i++) {
    //     // Skip inactive lights
    //     // if (light[i].intensity <= 0.0) continue;
    //     Lo += calcPointLight(light[i], N, WorldPos, V);
    // }

    for (int j=0; j<DIR_LIGHT_COUNT; j++) {
        // For directional lights, use the direction directly (not position-based)
        vec3 L = normalize(-dirLight[j].direction);

        // Halfway vector
        vec3 H = normalize(V + L);

        // Apply shadow calculation for directional lights
        float shadow = ShadowCalculation(FragPosLightSpace, N, L);

        // incoming light radiance scaled by intensity provided from glTF
        vec3 radiance = dirLight[j].color * dirLight[j].intensity;

        // BRDF Eq
        // ----------------------------------------------
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        // Normal Distribution
        float NDF = distributionGGX(N, H, roughness);

        // Geometry Distribution
        float geo = GeometrySmith(N, V, L, roughness);

        // specular calculation
        vec3 specular = cookTorrence(N, V, L, NDF, geo, F);

        vec3 kS = F;

        // metallic object does not refract so we nullify kD
        vec3 kD = vec3(1.0) -kS;
        kD *= (1.0 - metallic);

        float nDotL = max(dot(N, L), 0.0);

        // calculating  outgoing light radiance with shadow
        Lo  +=  ((kD * albedo / PI) + specular) * radiance * nDotL * (1.0 - shadow);
    }

    // irradiance (indirect lighting)
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 kS = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    // Energy compensation for rough specular (affect metals predominantly)
//     float energyBias   = 0.5 * roughness * metallic;
//     float energyFactor = 1.0 - 0.5 * roughness;
//     kS = kS * energyFactor + energyBias;

    vec3 kD = vec3(1.0) - kS;
    kD *= (1.0 - metallic);
    vec3 diffuse = irradiance * albedo;

    // Indirect specular reflection
    vec3 R = reflect(-V, N);
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor= textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 envBRDF = texture(brdfLUT, vec2(max(dot(N,V), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (kS * envBRDF.x + envBRDF.y);

    // float combinedAO = ao * ssaoFactor;
    // vec3 diffuseAmbient = kD * diffuse * combinedAO;
    // vec3 specularAmbient = specular * mix(combinedAO, 1.0, metallic); // Less AO on metals
    // vec3 ambient = diffuseAmbient + specularAmbient;

    // float combinedAO = ao * ssaoFactor;
    // vec3 ambient = (kD * diffuse + specular) * combinedAO;

    float ssaoModulation = mix(0.2, 1.0, ssaoFactor); // Minimum 30% ambient, maximum 100%
    vec3 ambient = (kD * diffuse * ssaoModulation + specular) * ao * ambientScale;


    vec3 color = ambient + Lo;

    // ACES filmic tone mapping with user-controlled exposure
    // reinhard tone mapping (As Lo can go very high to preserve the high dynamic range)
    color = tonemapACES(color * exposure);
    // color = color / (color + vec3(1.0));

    // gamma correction
    color = pow(color, vec3(1.0/2.2));

    // Use the alpha from baseColor for transparent materials
    float finalAlpha = baseColor.a;
    FragColor = vec4(color, finalAlpha);
}
