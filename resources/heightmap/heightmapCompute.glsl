#version 430 core

layout(local_size_x=16, local_size_y=16) in;

struct Vertex {
    vec3 position; float pad1;   // 16 bytes
    vec2 texCoord; vec2 pad2;    // 16 bytes (offset 16)
    vec3 normal;   float pad3;   // 16 bytes (offset 32)
};

layout(std430, binding=0) buffer VertexBuffer {
    Vertex vertices[];
};

uniform sampler2D heightMap;
uniform int terrainWidth;
uniform int terrainHeight;
uniform int gridSize;
uniform float heightScale;
uniform float heightShift;

void main() {
    uint x = gl_GlobalInvocationID.x;
    uint y = gl_GlobalInvocationID.y;
    if (x >= uint(gridSize) || y >= uint(gridSize)) return;

    // Normalized texture coordinates
    vec2 texCoord = vec2(float(x) / float(gridSize - 1), float(y) / float(gridSize - 1));
    // Sample and scale height
    float height = texture(heightMap, texCoord).r * heightScale - heightShift;

    // Centered grid positions
    float xPos = (float(x) / float(gridSize - 1)) * float(terrainWidth) - float(terrainWidth) * 0.5;
    float zPos = (float(y) / float(gridSize - 1)) * float(terrainHeight) - float(terrainHeight) * 0.5;

    uint index = y * uint(gridSize) + x;
    vertices[index].position = vec3(xPos, height, zPos);
    vertices[index].texCoord = texCoord;
    vertices[index].normal = vec3(0.0, 1.0, 0.0); // TODO: compute real normal
}
