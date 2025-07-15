#version 330 core
out vec4 FragColor;

in vec3 localPos;

uniform samplerCube environmentMap;
uniform float roughness;
const float PI = 3.1415926;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

// Van Der corpus sequence
// Radical inverse of i in base 2
// ex: i = 2
// Binary = 10
// Reverse = 01
// With binary point: 0.1(underscript 2) [In our case 32 as int size 32]
// (1*2^-1) = 0.5
float RadicalInverse_VdC(uint bits)
{
    // Swap first 16-bits with last 16-bits
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    // For 32 bit integer
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
// Hammersley sequence sample i over N total samples
vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    // uniformly distributed phi as pdf not dependent on phi
    float phi = 2.0 * PI * Xi.x;
    // calculated after inverse transform sampling
    float cosTheta = sqrt((1.0 - Xi.y) / (Xi.y * (a*a - 1.0) + 1.0));
    float sinTheta = sqrt(1 - (cosTheta * cosTheta));

    // from spherical coordinates to cartesian coordinates
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // from tangent space to world space
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = normalize(cross(N, tangent));

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

void main() {
    vec3 N = normalize(localPos);
    vec3 R = N;
    vec3 V = R;

    // Clamp roughness to avoid extreme aliasing and division-by-zero in PDF
    float r = max(roughness, 0.04);

    const uint SAMPLE_COUNT = 1024u;
    float totalWeight = 0.0;
    vec3 prefilterColor = vec3(0.0);

    for (uint i = 0u; i<SAMPLE_COUNT; ++i) {
        // get sample using hammersley sampling
        vec2 Xi  = Hammersley(i, SAMPLE_COUNT);
        // sample halfway vector using importance sampling which samples from specular probe
        vec3 H = ImportanceSampleGGX(Xi, N, r);
        // Getting outgoing outgoing
        vec3 L = normalize(2 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            // sample from the environment's mip level based on roughness/pdf
            float D   = DistributionGGX(N, H, roughness);
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float pdf = D * NdotH / (4.0 * HdotV) + 0.0001;

            float resolution = 512.0; //   resolution of source cubemap (per face)
            float saTexel  = 4.0 * PI / (6.0 * resolution * resolution);
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

            float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

            prefilterColor += textureLod(environmentMap, L, mipLevel).rgb * NdotL;
            totalWeight      += NdotL;

        }
    }
    prefilterColor = prefilterColor / totalWeight;
    FragColor = vec4(prefilterColor, 1.0);
}