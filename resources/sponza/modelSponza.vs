#version 330 core
layout (location = 0) in vec3 aPos;   
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords_0;
layout (location = 3) in vec2 aTexCoords_1;
layout (location = 4) in vec4 aTangent;

uniform mat4 MVP;
uniform mat4 lightSpaceMatrix;
uniform mat4 model;

out vec2 TexCoords;
out vec3 Normal;
out vec4 FragPosLightSpace;
out vec3 WorldPos;
out vec3 Tangent;
out vec3 Bitangent;


void main()
{
    TexCoords = aTexCoords_0;  // Use primary texture coordinates
    vec4 worldPos = model * vec4(aPos, 1.0f);
    WorldPos = worldPos.xyz;
    
    // Transform normal, tangent, and calculate bitangent to world space
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    Normal = normalize(normalMatrix * aNormal);
    Tangent = normalize(normalMatrix * aTangent.xyz);
    Bitangent = normalize(cross(Normal, Tangent)) * aTangent.w; // aTangent.w is handedness
    
    FragPosLightSpace = lightSpaceMatrix * worldPos;
    gl_Position = MVP * vec4(aPos, 1.0f);
}  