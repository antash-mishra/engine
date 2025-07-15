#version 330 core
out vec4 FragColor;

in vec3 localPos;

uniform sampler2D equirectangularMap;

// These constants are 1/(2*PI) and 1/PI
const vec2 invAtan = vec2(0.1591, 0.3183);

// Converts a 3D direction vector to 2D UV coordinates for an equirectangular map
vec2 SampleSphericalMap(vec3 v) {
    // atan(v.z, v.x) calculates the azimuthal angle (longitude, φ) [Horizontal Rotation]
    // asin(v.y) calculates the polar angle (latitude, θ) [vertical rotation]
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));

    // Scale and shift angle from [-PI, PI] and [-PI/2, PI/2] to texture coord range [0,1]
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    // 3D to 2D UV coordinates
    vec2 uv = SampleSphericalMap(normalize(localPos));

    // samples equirectangular image at those 2D coordinates
    vec3 color = texture(equirectangularMap, uv).rgb;

    FragColor = vec4(color, 1.0);
}