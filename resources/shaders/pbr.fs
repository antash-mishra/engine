#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
in vec3 WorldPos;
in vec3 Normal;

uniform vec3 camPosition;

uniform sampler2D albedoMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D normalMap;
uniform sampler2D aoMap;
uniform samplerCube irradianceMap;

// lights
uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];

const float PI = 3.14159265359;

vec3 getNormalFromNormalMap()
{
    vec3 tangentNormal = texture(normalMap, TexCoords).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(WorldPos);
    vec3 Q2  = dFdy(WorldPos);
    vec2 st1 = dFdx(TexCoords);
    vec2 st2 = dFdy(TexCoords);

    vec3 N   = normalize(Normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
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

void main() {
    vec3 albedo     = pow(texture(albedoMap, TexCoords).rgb, vec3(2.2));
    float metallic  = texture(metallicMap, TexCoords).r;
    float roughness = texture(roughnessMap, TexCoords).r;
    float ao        = texture(aoMap, TexCoords).r;



    vec3 N = getNormalFromNormalMap();

    // viewPos
    vec3 V = normalize(camPosition - WorldPos);
    vec3 Lo = vec3(0.0);

     // base reflectivity of surface
     vec3 F0 = vec3(0.04);
     F0 = mix(F0, albedo, metallic);

    for(int i=0; i<4; i++) {
        // light Direction
        vec3 L = normalize(lightPositions[i] - WorldPos);

        // Halfway vector
        vec3 H = normalize(V + L);

        // attenuation calculation
        float distance = length(lightPositions[i] - WorldPos);
        float attenuation  = 1.0 / (distance * distance);
        // incoming light Radiance
        vec3 radiance = lightColors[i] * attenuation;

        // BRDF Eq
        // ----------------------------------------------
        // Reflectance ratio
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

        // calculating  outgoing light radiance
        Lo  +=  ((kD * albedo / PI) + specular) * radiance * nDotL;
    }

     // vec3 ambient = vec3(0.03) * albedo * ao;

    // irradiance (indirect lighting)
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 kS = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kD = 1.0 - kS;
    kD *= (1.0 - metallic);
    vec3 diffuse = irradiance * albedo;
    vec3 ambient = (kD * diffuse) * ao;


    vec3 color = ambient + Lo;

    // reinhard tone mapping (As Lo can go very high to preserve the high dynamic range)
    color = color / (color + vec3(1.0));
    // gamma correction
    color = pow(color, vec3(1.0/2.2));
    FragColor = vec4(color, 1.0);
}