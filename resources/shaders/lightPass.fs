#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gColorSpec;

// Attenuation coefficients
const float LINEAR = 0.7;
const float QUADRATIC = 1.8;

struct Light {
    vec4 position;
    vec4 color;
    float intensity;
    float radius;
};

struct Cluster {
    vec4 minPoint;
    vec4 maxPoint;
    int lightCount;
    int lightIndices[100]; // Maximum of 100 lights per cluster
};

layout(std430, binding=1) restrict buffer clusterSSBO {
    Cluster cluster[];
};

layout(std430, binding=2) restrict buffer lightSSBO {
    Light Lights[];
};

uniform vec3 viewPos;
uniform vec2 screenSize;
uniform float zNear;
uniform float zFar;
uniform uvec3 gridSize;
uniform uvec2 screenDimension;

void main() {

    vec2 uv = TexCoords;
    vec3 FragPos = texture(gPosition, uv).rgb;
    
    // locating the cluster for this fragment
    uint zTile = uint((log(max(abs(FragPos.z), 0.0001) / zNear) * gridSize.z) / log(zFar / zNear));
    zTile = clamp(zTile, 0u, gridSize.z - 1u);
    vec2 tileSize = (screenDimension) / (gridSize.xy);
    uvec3 tile = uvec3(gl_FragCoord.xy / tileSize, zTile);
    uint tileIndex = tile.x + (tile.y * gridSize.x) + (tile.z * gridSize.x * gridSize.y);

    uint lightCount = uint(cluster[tileIndex].lightCount);

    vec3 Normal  = texture(gNormal, uv).rgb;
    vec3 Albedo  = texture(gColorSpec, uv).rgb;
    float SpecularStrength = texture(gColorSpec, uv).a;

    vec3 lighting = Albedo * 0.05; // lowered ambient

    for (uint i = 0u; i < lightCount; ++i) {
        uint lightIndex = cluster[tileIndex].lightIndices[i];
        Light light = Lights[lightIndex];

        // Frag has lights which are in the cluster
        // Calculate lighting for this single point light
        vec3 lightDir = light.position.xyz - FragPos;
        float distance = length(lightDir);
        lightDir = normalize(lightDir);
        float diff = max(dot(Normal, lightDir), 0.0);
        vec3 diffuse = diff * Albedo * light.color.rgb;

        // specular term 
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(Normal, halfwayDir), 0.0), 16.0);
        vec3 specular = spec * SpecularStrength * light.color.rgb; // updated to use light.color.rgb

        // Attenuation (quadratic fall-off)
        float attenuation = 1.0 / (1.0 + LINEAR * distance + QUADRATIC * distance * distance);

        diffuse *= attenuation;
        specular *= attenuation;

        lighting += diffuse + specular;

    }
    
    // Gamma correction (clamp to avoid NaN)
    lighting = max(lighting, vec3(0.0));
    vec3 gammaCorrected = pow(lighting, vec3(1.0/2.2));

    FragColor = vec4(gammaCorrected, 1.0);
}
