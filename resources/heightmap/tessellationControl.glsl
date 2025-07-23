#version 430 core

// vertex position
layout (vertices = 4) out;

// input from vertex shader
in vec2 TexCoord[];
// output to evaluation shader
out vec2 TextureCoord[];

// gl_InvocationID tracks vertex we are processing
// gl_in and gl_out struct
//in gl_PerVertex {
//    vec4 gl_Position;
//    float gl_PointSize;
//    float gl_ClipDistance;
//} gl_in[gl_MaxPatchVariables];

void main() {
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    TextureCoord[gl_InvocationID] = TexCoord[gl_InvocationID];

    // Number of tessellation to do
    if (gl_InvocationID == 0) {
        gl_TessLevelOuter[0] = 16;
        gl_TessLevelOuter[1] = 16;
        gl_TessLevelOuter[2] = 16;
        gl_TessLevelOuter[3] = 16;

        gl_TessLevelInner[0] = 16;
        gl_TessLevelInner[1] = 16;
    }
}