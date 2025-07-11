# Sponza Scene Implementation

The Sponza scene demonstrates a complete glTF 2.0 loading pipeline with advanced lighting and shadow mapping.

## Overview

The Sponza Atrium is a popular test scene in computer graphics research, featuring complex architecture with multiple materials, lighting scenarios, and geometric details perfect for testing rendering algorithms.

## Implementation Details

### Scene Loading (`GLTFLoader` class)

#### glTF 2.0 Support
- **TinyGLTF integration**: Full glTF 2.0 specification support
- **Binary (.glb) and ASCII (.gltf)** format loading
- **Embedded and external assets** handling
- **Performance timing**: Load time reporting for optimization

#### Asset Processing Pipeline
```cpp
bool GLTFLoader::loadModel(const std::string &filename) {
    // 1. File parsing (JSON + binary data)
    // 2. Texture processing and GPU upload
    // 3. Material property extraction
    // 4. Light data parsing (KHR_lights_punctual)
    // 5. Mesh geometry processing
    // 6. Scene graph node construction
}
```

### Materials & Textures

#### PBR Material Support
- **Base Color**: Albedo/diffuse textures with tint factors
- **Normal Maps**: Tangent-space surface detail
- **Metallic-Roughness**: Material property textures
- **Emissive**: Self-illuminating surfaces

#### Texture Loading Optimizations
```cpp
void ProcessTextures() {
    // Efficient GPU upload with proper format detection
    // Mipmap generation for filtering
    // Sampler state configuration from glTF
}
```

### Lighting System

#### KHR_lights_punctual Extension
The implementation supports the official glTF lighting extension:

```json
"extensions": {
  "KHR_lights_punctual": {
    "lights": [
      {
        "type": "directional",
        "color": [0.9999999, 0.9999998, 1.0],
        "intensity": 3.14159
      }
    ]
  }
}
```

#### Light Processing
```cpp
void ProcessLights() {
    // Extract light definitions from extension
    // Parse type (directional/point/spot)
    // Store color and intensity values
    // Handle disabled lights (intensity = 0)
}
```

#### Scene Light Setup
```cpp
void setupLighting(Shader shaderProgram) {
    // Collect active lights from scene nodes
    // Calculate world-space positions and directions
    // Upload to shader uniform arrays
    // Support up to 24 lights simultaneously
}
```

### Shadow Mapping

#### Directional Light Shadows
The system implements cascaded shadow mapping for the main directional light:

```cpp
// Extract light transform from glTF scene
bool getFirstDirectionalLight(glm::vec3 &outPosition, glm::vec3 &outDirection) {
    // Find first enabled directional light
    // Extract position from node transform
    // Calculate direction from rotation matrix
}
```

#### Shadow Map Generation
```glsl
// Depth-only vertex shader
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 MVP;
void main() {
    gl_Position = MVP * vec4(aPos, 1.0);
}
```

#### Shadow Map Sampling
```glsl
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // Perspective divide and [0,1] mapping
    // PCF (Percentage Closer Filtering) with 3x3 kernel
    // Normal-based bias to prevent shadow acne
    // Outside frustum handling
}
```

### Scene Graph & Rendering

#### Node Hierarchy
```cpp
struct Node {
    std::vector<int> meshIndices;    // Geometry references
    std::vector<int> children;       // Child node indices
    glm::mat4 transform;            // Local transform
    Light light;                    // Optional light data
    bool hasLight;                  // Light presence flag
};
```

#### Recursive Rendering
```cpp
void RenderNodes(int nodeIndex, Shader shader, 
                const glm::mat4& view, const glm::mat4& projection,
                const glm::mat4& parentTransform) {
    // Compose transform hierarchy
    // Set MVP and model matrices
    // Bind material textures
    // Render geometry
    // Optional: Debug light cube visualization
    // Recurse to children
}
```

### Shader Integration

#### Vertex Shader (`modelSponza.vs`)
```glsl
// Transform vertex to world space
vec4 worldPos = model * vec4(aPos, 1.0);
FragPos = worldPos.xyz;

// Calculate light-space position for shadows
FragPosLightSpace = lightSpaceMatrix * worldPos;

// Transform normal to world space (TODO: use normal matrix)
Normal = aNormal;
```

#### Fragment Shader (`modelSponza.fs`)
```glsl
// Directional light with shadows
vec3 lightDir = normalize(-dirLight[j].direction);
float diff = max(dot(lightDir, normal), 0.0);
vec3 diffuse = diff * dirLight[j].color;

// Shadow calculation
float shadow = ShadowCalculation(FragPosLightSpace, normal, lightDir);
lighting += (ambient + (1.0 - shadow) * (diffuse + specular)) * 0.4;
```

## Performance Characteristics

### Loading Performance
- **File Size**: ~2.6GB for full Sponza with high-res textures
- **Load Time**: ~78 seconds for full asset version
- **Optimization**: Use smaller Sponza variants for development

### Runtime Performance
- **Shadow Map Resolution**: 1024x1024 with PCF
- **Material Complexity**: Full PBR workflow support
- **Light Count**: Up to 24 simultaneous lights
- **Geometry**: Complex architectural meshes with high vertex counts

## Debug Features

### Light Visualization
```cpp
// Toggle with 'L' key
if (showLightCubes && node.hasLight) {
    renderLightCube(model, shaderProgram, view, projection, node.light.color);
}
```

### Shadow Map Debugging
```cpp
// Visual shadow map inspection
debugDepthQuadShader.use();
debugDepthQuadShader.setFloat("near_plane", near_plane);
debugDepthQuadShader.setFloat("far_plane", far_plane);
glBindTexture(GL_TEXTURE_2D, depthMap);
```

## Asset Structure

### File Organization
```
resources/main-sponza/main_sponza/
├── NewSponza_Main_glTF_003.gltf    # Scene description
├── NewSponza_Main_glTF_003.bin     # Binary geometry data
└── textures/                       # PBR texture maps
    ├── *_BaseColor.png             # Albedo textures
    ├── *_Normal.png                # Normal maps
    ├── *_Metalness.png             # Metallic maps
    └── *_Roughness.png             # Roughness maps
```

### Light Configuration
The scene includes multiple disabled lights (intensity = 0) which can be enabled by:
1. Editing the glTF file directly
2. Modifying intensity values in the loader
3. Implementing runtime light control

## Common Issues & Solutions

### Shadow Acne
```glsl
// Dynamic bias based on surface angle
float bias = max(0.3 * (1.0 - dot(normal, lightDir)), 0.03);
```

### Peter Panning
```cpp
// Use front-face culling during shadow map generation
glCullFace(GL_FRONT);
loader.Render(depthShader, lightView, lightProjection);
glCullFace(GL_BACK);
```

### Light Transformation
```cpp
// Extract position and direction from node transform
glm::vec3 lightPos = glm::vec3(model[3]);
glm::vec3 lightDir = glm::normalize(glm::mat3(model) * glm::vec3(0.0f, 0.0f, -1.0f));
```

## Future Enhancements

1. **Cascaded Shadow Maps**: Multiple shadow map resolutions
2. **Temporal Filtering**: Shadow map temporal stability
3. **Light Probes**: Pre-computed indirect illumination
4. **Level-of-Detail**: Automatic geometry simplification