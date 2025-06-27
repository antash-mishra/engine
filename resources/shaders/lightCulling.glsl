#version 430 core

#define LOCAL_SIZE 128
layout (local_size_x = LOCAL_SIZE, local_size_y=1, local_size_z=1) in;

struct PointLight {
    vec4 position;
    vec4 color;
    float intensity;
    float radius;
};

struct Cluster {
    vec4 minPoint;
    vec4 maxPoint;
    uint count;
    uint lightIndices[100];
};

layout(std430, binding=1) restrict buffer clusterSSBO {
    Cluster clusters[];
};

layout(std430, binding=2) restrict buffer lightSSBO {
    PointLight pointLight[];
};

uniform mat4 viewMatrix;

bool testSphereAABB(uint i, Cluster c);

void main() {
    uint lightCount = pointLight.length();
    uint index = gl_WorkGroupID.x * LOCAL_SIZE + gl_LocalInvocationID.x;
    Cluster cluster = clusters[index];

    // reset light count as culling runs every frame
    // if not reset, the light count will accumulate
    cluster.count = 0;

    for (uint i = 0; i < lightCount; i++) {
        // keep threshold of 100 lights per cluster
        if (testSphereAABB(i, cluster) && cluster.count < 100) {
            cluster.lightIndices[cluster.count] = i;
            cluster.count++;
        }
    }

    clusters[index] = cluster;
}

bool sphereAABBIntersection(vec3 center, float radius, vec3 aabbMin, vec3 aabbMax) {
    // Find the closest point to the sphere within the AABB
    vec3 closestPoint = clamp(center, aabbMin, aabbMax);
    // Calculate the squared distance from the closest point to the sphere's center 
    // and compare it to the radius squared
    // to avoid a square root operation
    float distance = dot(closestPoint - center, closestPoint - center);
    // If the distance is less than or equal to the radius, there is an intersection
    return distance <= radius * radius;
}

bool testSphereAABB(uint i, Cluster c) {
    float radius = pointLight[i].radius;
    vec3 center = vec3(viewMatrix * pointLight[i].position);

    vec3 aabbMin = c.minPoint.xyz;
    vec3 aabbMax = c.maxPoint.xyz;
    return sphereAABBIntersection(center, radius, aabbMin, aabbMax);
}
