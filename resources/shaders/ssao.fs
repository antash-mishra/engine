#version 430 core
out float FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform vec3 samples[64];
uniform mat4 projection;
uniform vec2 noiseScale;

const float bias = 0.025;
const float radius = 0.5;

void main() {
    vec3 FragPos = texture(gPosition, TexCoords).xyz;
    vec3 normal = texture(gNormal, TexCoords).xyz;
    vec3 randomNoiseVec = texture(texNoise, TexCoords * noiseScale).xyz;

    // moving samples in the kernel from tangent space to view-space using noise vec
    vec3 tangent = normalize(randomNoiseVec - normal * dot(randomNoiseVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i=0; i<samples.length(); ++i){
        // get sample position
        // from tangent to view space
        vec3 samplePos = TBN * samples[i];
        // adding to view-space fragPos and then multiplying by radius to increase the extent of occlusion sampling
        samplePos = FragPos + samplePos * radius;

        // samplePos from view-space to screen-space
        vec4 offset = vec4(samplePos, 1.0);
        offset = projection * offset; // view to projection/clip space
        offset.xyz = offset.xyz/offset.w; // clip to ndc space [-1, 1]
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0

        float sampleDepth = texture(gPosition, offset.xy).z;
        float rangeCheck = smoothstep(0.0, 1.0, radius/ abs(FragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / 64);
    FragColor = occlusion;
}
