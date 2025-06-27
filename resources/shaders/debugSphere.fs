#version 430 core
out vec4 FragColor;

uniform vec3  sphereColor;
uniform float alpha;          // how transparent you want it (e.g. 0.25)

void main()
{
    FragColor = vec4(vec3(1.0), 1.0);
}