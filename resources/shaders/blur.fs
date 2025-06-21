#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D image;

uniform bool horizontal;
uniform float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {

   vec2 tex_offsets = 1.0/textureSize(image, 0);  // size of 1 pixel
   vec3 result = texture(image, TexCoords).rgb * weight[0]; // center pixel

   if (horizontal) {
        for (int i=1; i<5; ++i) {
            // sample pixels to left and right
            result += texture(image, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weight[i]
            result += texture(image, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weigth[i]
        }
   }
   else {
        for (int i = 1; i<5; i++) {
            // sample pixels above and below
            result += texture(image, TexCoords + vec2(tex_offset.y * i, 0.0)).rgb * weight[i]
            result += texture(image, TexCoords - vec2(tex_offset.y * i, 0.0)).rgb * weigth[i]
        }
   }
   FragColor = vec4(result, 1.0);
}
