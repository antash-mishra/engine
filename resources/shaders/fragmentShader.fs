#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D texture_diffuse0;
// uniform sampler2D texture_specular0;

uniform vec3 viewPos;
uniform vec3 lightPos;


void main()
{

    FragColor = vec4(texture(texture_diffuse0, TexCoords).rgb, 1.0);
}