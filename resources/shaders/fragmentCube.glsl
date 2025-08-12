#version 430 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;

uniform vec3 cameraPosition;
uniform vec2 iResolution;
uniform float iTime;
float distance_from_sphere(in vec3 p, in vec3 c, float r) {
    return length(p - c) - r;
}

float map_to_world(in vec3 p) {
    float displacement = sin(5.0 * (p.x + sin(iTime))) * sin(5.0 * (p.y + sin(iTime))) * sin(5.0 * (p.z - cos(iTime))) * 0.25;
    float sphere_0 = distance_from_sphere(p, vec3(.0), 1.0);
    return sphere_0 + displacement;
}

vec3 calculate_normal(in vec3 p) {
    const vec3 small_step = vec3(0.001, 0.0, 0.0);

    float gradient_x = map_to_world(p + small_step.xyy) - map_to_world(p - small_step.xyy);
    float gradient_y = map_to_world(p + small_step.yxy) - map_to_world(p - small_step.yxy);
    float gradient_z = map_to_world(p - small_step.yyx) - map_to_world(p + small_step.yyx);
    vec3 normal = vec3(gradient_x, gradient_y, gradient_z);
    return normalize(normal);
}

vec3 ray_march(in vec3 ro, in vec3 rd) {
    float distance_traveled = 0.0;
    const int NUM_OF_STEPS = 32;
    const float MINIMUM_HIT_DISTANCE = 0.001;
    const float MAXIMUM_TRACE_DISTANCE = 1000.0;

    for (int i=0; i< NUM_OF_STEPS; i++) {
        vec3 current_position = ro + distance_traveled * rd;
        float distance_to_closest = map_to_world(current_position);

        if (distance_to_closest < MINIMUM_HIT_DISTANCE) {
            vec3 normal = calculate_normal(current_position);
            // For now, hard-code the light's position in our scene
            vec3 light_position = vec3(2.0, -5.0, 3.0);

            // Calculate the unit direction vector that points from
            // the point of intersection to the light source
            vec3 direction_to_light = normalize(current_position - light_position);

            float diffuse_intensity = max(0.0, dot(normal, direction_to_light));

            return vec3(1.0, 0.0, 0.0) * diffuse_intensity;

        }

        if (distance_traveled > MAXIMUM_TRACE_DISTANCE) {
            break;
        }
        distance_traveled += distance_to_closest;
    }
    return vec3(0.0);
}

void main()
{
    vec2 uv =  (TexCoords * iResolution.xy - 0.5 * iResolution.xy) / (0.5 * iResolution.y);
//    vec2 uv = TexCoords * 2.0 - 1.0;
//    vec2 uv = TexCoords;
    vec3 camera_postion = cameraPosition;
    vec3 ro = camera_postion;
    vec3 rd = normalize(vec3(uv,-1.0));
    vec3 shaded_color = ray_march(ro, rd);
    FragColor = vec4(shaded_color, 1.0);
}