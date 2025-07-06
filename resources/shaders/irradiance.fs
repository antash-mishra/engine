#version 330 core

out vec4 FragColor;
in vec3 localPos;

uniform samplerCube environmentMap;

const float PI = 3.14159;

void main() {
    vec3 normal = normalize(localPos);
    vec3 irradiance = vec3(0.0);

    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right  = normalize(vec3(cross(up, normal)));
    up =normalize(cross(normal, right));

    float sampleDelta = 0.025;
    float nrSamples = 0.0;

    // Looping over phi and theta and summing up
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < PI / 2.0; theta += sampleDelta) {
            // spherical to cartesian(in tangent space)
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent to world space
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
            irradiance += texture(environmentMap, sampleVec).rgb * cos(theta)*sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    FragColor = vec4(irradiance, 1.0);

}