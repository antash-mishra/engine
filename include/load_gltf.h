#ifndef GLTFLOADER_H
#define GLTFLOADER_H

// TinyGLTF single-header implementation helpers for loading and rendering glTF
// models with OpenGL.  All functions live in the load_gltf namespace.
//
// Usage:
//   #include "load_gltf.h"
//   tinygltf::Model model;
//   load_gltf::loadModel(model, "scene.gltf");
//   auto vaoPack = load_gltf::bindModel(model);
//   ... during render loop ...
//   load_gltf::drawModel(vaoPack, model);
//
// The helper allocates VAOs/VBOs for attributes & index buffers and stores them
// in a map so they can be cleaned up later by the caller.
// ---------------------------------------------------------------------------
#include <glad/glad.h>
#include "glm/detail/type_vec.hpp"
#include "shader.h"
#include <tiny_gltf.h>
#include <map>
#include <unordered_set>
#include <cassert>
#include <utility>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <chrono>

#define GLT_BUFFER_OFFSET(i) ((char *)nullptr + (i))


enum lightType {
    POINT,
    DIRECTIONAL
};


struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords_0;
    glm::vec2 texCoords_1;
    glm::vec4 tangent;  // xyz = tangent direction, w = handedness
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    int materialIndex;
    unsigned int VAO, VBO, EBO;
};

struct Light {
    glm::vec3 color;
    float intensity;
    enum lightType  type;
};

struct Material {
    glm::vec4 baseColorFactor;
    int baseColorTexture;
    int normalTexture;
    int metallicRoughnessTexture;
    int occlusionTexture;
    float metallicFactor;
    float roughnessFactor;
};


struct Texture {
    unsigned int textureID;
    int samplerIndex;
};

struct Node {
    std::vector<int> meshIndices;
    std::vector<int> children;
    glm::mat4 transform;
    glm::mat4 worldTransform;
    Light light;
    bool hasLight = false;
};



class GLTFLoader {
    public:
        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        std::vector<Texture> textures;
        std::vector<Node> nodes;
        std::vector<Light> lights;
        tinygltf::Model model;
        
        // Debug visualization options
        bool showLightCubes = true;

        // Load a glTF/GLB file into the provided tinygltf::Model.
        // Returns true on success.
        bool loadModel(const std::string &filename) {
            std::cout << "loadModel" << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            
            tinygltf::TinyGLTF loader;
            std::string err;
            std::string warn;

            bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
            if (!warn.empty()) {
                std::cout << "WARN: " << warn << std::endl;
            }

            if (!err.empty()) {
                std::cout << "ERR: " << err << std::endl;
            }

            if (!res) {
                std::cout << "Failed to load glTF: " << filename << std::endl;
                return false;
            }
            
            auto loadTime = std::chrono::high_resolution_clock::now();
            std::cout << "File loaded in " << std::chrono::duration_cast<std::chrono::milliseconds>(loadTime - start).count() << "ms" << std::endl;

            auto processStart = std::chrono::high_resolution_clock::now();
            ProcessTextures();
            auto textureTime = std::chrono::high_resolution_clock::now();
            std::cout << "Textures processed in " << std::chrono::duration_cast<std::chrono::milliseconds>(textureTime - processStart).count() << "ms" << std::endl;
            
            ProcessMaterials();
            auto materialTime = std::chrono::high_resolution_clock::now();
            std::cout << "Materials processed in " << std::chrono::duration_cast<std::chrono::milliseconds>(materialTime - textureTime).count() << "ms" << std::endl;

            ProcessLights();
            auto lightTime = std::chrono::high_resolution_clock::now();
            std::cout << "Lights processed in " << std::chrono::duration_cast<std::chrono::milliseconds>(lightTime - materialTime).count() << "ms" << std::endl;

            ProcessMeshes();
            auto meshTime = std::chrono::high_resolution_clock::now();
            std::cout << "Meshes processed in " << std::chrono::duration_cast<std::chrono::milliseconds>(meshTime - lightTime).count() << "ms" << std::endl;
            
            ProcessNodes();
            auto nodeTime = std::chrono::high_resolution_clock::now();
            std::cout << "Nodes processed in " << std::chrono::duration_cast<std::chrono::milliseconds>(nodeTime - meshTime).count() << "ms" << std::endl;
            
            auto totalTime = std::chrono::high_resolution_clock::now();
            std::cout << "Total loading time: " << std::chrono::duration_cast<std::chrono::milliseconds>(totalTime - start).count() << "ms" << std::endl;
            
            return true;
        }

    private:
        // Debug cube for light visualization
        unsigned int lightCubeVAO = 0, lightCubeVBO = 0;
        
        void createLightCube() {
            if (lightCubeVAO == 0) {
                float cubeVertices[] = {
                    // positions
                    -0.5f, -0.5f, -0.5f,
                     0.5f, -0.5f, -0.5f,
                     0.5f,  0.5f, -0.5f,
                     0.5f,  0.5f, -0.5f,
                    -0.5f,  0.5f, -0.5f,
                    -0.5f, -0.5f, -0.5f,

                    -0.5f, -0.5f,  0.5f,
                     0.5f, -0.5f,  0.5f,
                     0.5f,  0.5f,  0.5f,
                     0.5f,  0.5f,  0.5f,
                    -0.5f,  0.5f,  0.5f,
                    -0.5f, -0.5f,  0.5f,

                    -0.5f,  0.5f,  0.5f,
                    -0.5f,  0.5f, -0.5f,
                    -0.5f, -0.5f, -0.5f,
                    -0.5f, -0.5f, -0.5f,
                    -0.5f, -0.5f,  0.5f,
                    -0.5f,  0.5f,  0.5f,

                     0.5f,  0.5f,  0.5f,
                     0.5f,  0.5f, -0.5f,
                     0.5f, -0.5f, -0.5f,
                     0.5f, -0.5f, -0.5f,
                     0.5f, -0.5f,  0.5f,
                     0.5f,  0.5f,  0.5f,

                    -0.5f, -0.5f, -0.5f,
                     0.5f, -0.5f, -0.5f,
                     0.5f, -0.5f,  0.5f,
                     0.5f, -0.5f,  0.5f,
                    -0.5f, -0.5f,  0.5f,
                    -0.5f, -0.5f, -0.5f,

                    -0.5f,  0.5f, -0.5f,
                     0.5f,  0.5f, -0.5f,
                     0.5f,  0.5f,  0.5f,
                     0.5f,  0.5f,  0.5f,
                    -0.5f,  0.5f,  0.5f,
                    -0.5f,  0.5f, -0.5f
                };

                glGenVertexArrays(1, &lightCubeVAO);
                glGenBuffers(1, &lightCubeVBO);

                glBindBuffer(GL_ARRAY_BUFFER, lightCubeVBO);
                glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

                glBindVertexArray(lightCubeVAO);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
                glEnableVertexAttribArray(0);

                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(0);
            }
        }

        void renderLightCube(const glm::mat4& lightTransform, Shader& shader, const glm::mat4& view, const glm::mat4& projection, const glm::vec3& lightColor) {
            createLightCube();
            
            // Create a small cube at light position
            glm::mat4 cubeModel = lightTransform;
            cubeModel = glm::scale(cubeModel, glm::vec3(0.5f)); // Make it smaller (0.5x0.5x0.5 units)
            
            glm::mat4 MVP = projection * view * cubeModel;
            shader.setMat4("MVP", MVP);
            
            // Bind a 1x1 white texture so baseColorTexture sample returns white (avoid black cube)
            static unsigned int whiteTex = 0;
            if (whiteTex == 0) {
                unsigned char whitePixel[3] = {255, 255, 255};
                glGenTextures(1, &whiteTex);
                glBindTexture(GL_TEXTURE_2D, whiteTex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, whitePixel);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            }
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, whiteTex);
            shader.setInt("baseColorTexture", 0);

            // Set light[0] properties to illuminate the cube with its own color
            shader.setVec3("light[0].direction", glm::vec3(0.0f, -1.0f, 0.0f));
            shader.setVec3("light[0].color", lightColor);
            shader.setFloat("light[0].intensity", 1.0f);
            shader.setBool("light[0].isPointLight", false);
            
            glBindVertexArray(lightCubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }

        std::vector<glm::vec4> GetVec4Data(const tinygltf::Accessor& accessor) {
            std::vector<glm::vec4> data;

            const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

            const float *ptr = reinterpret_cast<const float*>(
                buffer.data.data() + bufferView.byteOffset + accessor.byteOffset
            );

            size_t stride = bufferView.byteStride;
            size_t byteStride = stride? stride : sizeof(float) * 4;

            // Validate accessor bounds
            if (accessor.minValues.size() == 4 && accessor.maxValues.size() == 4) {
                // Optional: Could validate that loaded values are within bounds
                std::cout << "Accessor bounds: min(" << accessor.minValues[0] << ", " << accessor.minValues[1] 
                         << ", " << accessor.minValues[2] << ", " << accessor.minValues[3] << "), max(" 
                         << accessor.maxValues[0] << ", " << accessor.maxValues[1] << ", " << accessor.maxValues[2] 
                         << ", " << accessor.maxValues[3] << ")" << std::endl;
            }

            for (size_t i = 0; i < accessor.count; i++) {
                const float *vertexPtr = reinterpret_cast<const float*>(
                    reinterpret_cast<const char*>(ptr) + i * byteStride
                );
                data.push_back(glm::vec4(vertexPtr[0], vertexPtr[1], vertexPtr[2], vertexPtr[3]));
            }

            return data;
        }

        std::vector<glm::vec3> GetVec3Data(const tinygltf::Accessor& accessor) {
            std::vector<glm::vec3> data;

            const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

            const float *ptr = reinterpret_cast<const float*>(
                buffer.data.data() + bufferView.byteOffset + accessor.byteOffset
            );

            size_t stride = bufferView.byteStride;
            size_t byteStride = stride? stride : sizeof(float) * 3;

            for (size_t i = 0; i < accessor.count; i++) {
                const float *vertexPtr = reinterpret_cast<const float*>(
                    reinterpret_cast<const char*>(ptr) + i * byteStride
                );
                data.push_back(glm::vec3(vertexPtr[0], vertexPtr[1], vertexPtr[2]));
            }

            return data;
        }

        std::vector<glm::vec2> GetVec2Data(const tinygltf::Accessor& accessor) {
            std::vector<glm::vec2> data;

            const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

            const float *ptr = reinterpret_cast<const float*>(
                buffer.data.data() + bufferView.byteOffset + accessor.byteOffset
            );

            size_t stride = bufferView.byteStride;
            size_t byteStride = stride? stride : sizeof(float) * 2;

            for (size_t i = 0; i < accessor.count; i++) {
                const float *vertexPtr = reinterpret_cast<const float*>(
                    reinterpret_cast<const char*>(ptr) + i * byteStride
                );
                data.push_back(glm::vec2(vertexPtr[0], vertexPtr[1]));
            }

            return data;
        }

        std::vector<uint32_t> GetIndices(const tinygltf::Accessor& accessor) {
            std::vector<uint32_t> indices;

            const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

            const void *ptr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;



            // Handle different index type
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                const auto *shortPtr = static_cast<const uint16_t*>(ptr);
                for (size_t i = 0; i < accessor.count; i++) {
                    indices.push_back(static_cast<uint32_t>(shortPtr[i]));
                }
            }

            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                const auto *shortPtr = static_cast<const uint32_t*>(ptr);
                for (size_t i = 0; i < accessor.count; i++) {
                    indices.push_back(static_cast<uint32_t>(shortPtr[i]));
                }
            }

            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                const auto *shortPtr = static_cast<const uint8_t*>(ptr);
                for (size_t i = 0; i < accessor.count; i++) {
                    indices.push_back(static_cast<uint32_t>(shortPtr[i]));
                }
            }

            return indices;
        }

        void CreateBuffers(Mesh& mesh) {
            glGenVertexArrays(1, &mesh.VAO);
            glGenBuffers(1, &mesh.VBO);
            glGenBuffers(1, &mesh.EBO);

            glBindVertexArray(mesh.VAO);

            glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
            glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vertex), mesh.vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t), mesh.indices.data(), GL_STATIC_DRAW);

            // set vertex attributes
            // Position
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

            // Normal
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

            // TexCoords_0 (primary texture coordinates)
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords_0));

            // TexCoords_1 (secondary texture coordinates)
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords_1));

            // Tangent
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));


            glBindVertexArray(0);
        }

        void ProcessPrimitive(const tinygltf::Primitive& primitive, Mesh& mesh) {
            std::vector<glm::vec3> positions;
            std::vector<glm::vec3> normals;
            std::vector<glm::vec2> texCoords_0;
            std::vector<glm::vec2> texCoords_1;
            std::vector<glm::vec4> tangents;

            // Extract position
            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("POSITION")];
                positions = GetVec3Data(accessor);
            }

            // Extract texCoords
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                texCoords_0 = GetVec2Data(accessor);
            }

            // Extract texCoords
            if (primitive.attributes.find("TEXCOORD_1") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_1")];
                texCoords_1 = GetVec2Data(accessor);
            }

            // Extract Normal
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
                normals = GetVec3Data(accessor);
            }

            // Extract Tangent
            if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TANGENT")];
                tangents = GetVec4Data(accessor);
            }

            // Build vertex Array
            size_t vertexCount = positions.size();
            mesh.vertices.resize(vertexCount);

            for (size_t v = 0; v < vertexCount; v++) {
                mesh.vertices[v].position = positions[v];
                mesh.vertices[v].normal = (v < normals.size()) ? normals[v] : glm::vec3(0.0f);
                // If the primitive does not provide TEXCOORD_0 (common for decals) but does provide TEXCOORD_1,
                // fall back to that secondary UV set so the fragment shader receives meaningful coordinates.
                if (v < texCoords_0.size()) {
                    mesh.vertices[v].texCoords_0 = texCoords_0[v];
                } else if (v < texCoords_1.size()) {
                    mesh.vertices[v].texCoords_0 = texCoords_1[v];
                } else {
                    // Absolute fallback – centre of the texture.  Should not happen for real geometry.
                    mesh.vertices[v].texCoords_0 = glm::vec2(0.5f);
                }
                mesh.vertices[v].texCoords_1 = (v < texCoords_1.size()) ? texCoords_1[v] : glm::vec2(0.0f);
                mesh.vertices[v].tangent = (v < tangents.size()) ? glm::vec4(tangents[v]) : glm::vec4(0.0f);
            }

            // Extract indices
            if (primitive.indices  >= 0) {
                const tinygltf::Accessor &accessor = model.accessors[primitive.indices];
                mesh.indices  = GetIndices(accessor);
            }

            // Get material index
            mesh.materialIndex = primitive.material;

            // Create buffer
            CreateBuffers(mesh);
        }


        void ProcessTextures() {
            std::cout << "Processing " << model.textures.size() << " textures..." << std::endl;
            textures.resize(model.textures.size());
            int loadedCount = 0;
            // -----------------------------------------------------------------
            // Determine which texture indices should be treated as sRGB
            // (base-color and emissive are defined in sRGB space)
            // -----------------------------------------------------------------
            std::unordered_set<int> sRGBImages;
            for (const auto &mat : model.materials) {
                if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0)
                    sRGBImages.insert(model.textures[mat.pbrMetallicRoughness.baseColorTexture.index].source);
                if (mat.emissiveTexture.index >= 0)
                    sRGBImages.insert(model.textures[mat.emissiveTexture.index].source);
            }
            for (size_t t = 0; t < model.textures.size(); t++) {
                const tinygltf::Texture& gltfTexture = model.textures[t];
                // std::cout << "Texture " << t << ": source=" << gltfTexture.source << ", sampler=" << gltfTexture.sampler << std::endl;
                
                if (gltfTexture.source >= 0) {
                    const tinygltf::Image& gltfImage = model.images[gltfTexture.source];
                    
                    // create Texture
                    GLuint texID;
                    glGenTextures(1, &texID);
                    glBindTexture(GL_TEXTURE_2D, texID);
                    GLenum external;
                    if (gltfImage.component == 1) external = GL_RED;
                    else if (gltfImage.component == 2) external = GL_RG;
                    else if (gltfImage.component == 3) external = GL_RGB;
                    else external = GL_RGBA; // 4
                    
                    bool needsSRGB = sRGBImages.count(gltfTexture.source) > 0;
                    GLenum internal;
                    switch (gltfImage.component) {
                        case 1: internal = GL_R8; break;
                        case 2: internal = GL_RG8; break;
                        case 3: internal = needsSRGB ? GL_SRGB8 : GL_RGB8; break;
                        case 4: default: internal = needsSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8; break;
                    }
                    glTexImage2D(GL_TEXTURE_2D, 0, internal, gltfImage.width, gltfImage.height, 0, external, GL_UNSIGNED_BYTE,  gltfImage.image.data());
                    GLenum error = glGetError();
                    if (error != GL_NO_ERROR) {
                        std::cout << "OpenGL error after glTexImage2D for texture " << t << ": " << error << std::endl;
                    }
                    
                    // Apply sampler settings if one is specified, otherwise use glTF defaults
                    if (gltfTexture.sampler >= 0 && gltfTexture.sampler < model.samplers.size()) {
                        const tinygltf::Sampler& sampler = model.samplers[gltfTexture.sampler];
                        GLint minF = sampler.minFilter >= 0 ? sampler.minFilter : GL_LINEAR_MIPMAP_LINEAR;
                        GLint magF = sampler.magFilter >= 0 ? sampler.magFilter : GL_LINEAR;
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minF);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magF);
                        GLint wrapS = sampler.wrapS ? sampler.wrapS : GL_REPEAT;
                        GLint wrapT = sampler.wrapT ? sampler.wrapT : GL_REPEAT;
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    } else {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    }
                    error = glGetError();
                    if (error != GL_NO_ERROR) {
                        std::cout << "OpenGL error after glTexParameteri for texture " << t << ": " << error << std::endl;
                    }
                    glGenerateMipmap(GL_TEXTURE_2D);
                    error = glGetError();
                    if (error != GL_NO_ERROR) {
                        std::cout << "OpenGL error after glGenerateMipmap for texture " << t << ": " << error << std::endl;
                    }
                    textures[t].textureID = texID;
                    textures[t].samplerIndex = gltfTexture.sampler;
                    loadedCount++;
                } else {
                    std::cout << "Texture " << t << " has no valid source, not loaded." << std::endl;
                }
            }
            std::cout << "Successfully loaded " << loadedCount << "/" << textures.size() << " textures" << std::endl;
        }

        void ProcessMaterials() {
            std::cout << "Processing materials" << std::endl;
            materials.resize(model.materials.size());
            for (size_t m = 0; m < model.materials.size(); m++) {
                const tinygltf::Material& gltfMaterial = model.materials[m];
                Material& material = materials[m];
                // pbr metallic roughness workflow
                // base color
                if (gltfMaterial.pbrMetallicRoughness.baseColorFactor.size() == 4) {
                    material.baseColorFactor = glm::vec4(
                        gltfMaterial.pbrMetallicRoughness.baseColorFactor[0],
                        gltfMaterial.pbrMetallicRoughness.baseColorFactor[1],
                        gltfMaterial.pbrMetallicRoughness.baseColorFactor[2],
                        gltfMaterial.pbrMetallicRoughness.baseColorFactor[3]
                    );
                }
                else {
                    material.baseColorFactor = glm::vec4(1.0f);
                }

                // metallic factor - check if this material has explicit values or is using defaults
                // If no metallic-roughness texture is specified and values are at defaults, use more reasonable values
                bool hasMetallicRoughnessTexture = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0;
                
                if (hasMetallicRoughnessTexture) {
                    // Use the factor values as-is when texture is present
                    material.metallicFactor = gltfMaterial.pbrMetallicRoughness.metallicFactor;
                    material.roughnessFactor = gltfMaterial.pbrMetallicRoughness.roughnessFactor;
                } else {
                    // No texture - use more sensible defaults for most materials
                    material.metallicFactor = (gltfMaterial.pbrMetallicRoughness.metallicFactor == 1.0) ? 0.0f : gltfMaterial.pbrMetallicRoughness.metallicFactor;
                    material.roughnessFactor = (gltfMaterial.pbrMetallicRoughness.roughnessFactor == 1.0) ? 0.5f : gltfMaterial.pbrMetallicRoughness.roughnessFactor;
                }

                // Texture indices
                material.baseColorTexture  = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
                material.normalTexture = gltfMaterial.normalTexture.index;
                material.metallicRoughnessTexture = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
                if (gltfMaterial.occlusionTexture.index >= 0)
                    material.occlusionTexture = gltfMaterial.occlusionTexture.index;
                else
                    material.occlusionTexture = -1;
            }
        }

        void ProcessLights() {
            
            // Check if the KHR_lights_punctual extension exists
            if (model.extensions.find("KHR_lights_punctual") == model.extensions.end()) {
                std::cout << "No KHR_lights_punctual extension found" << std::endl;
                return;
            }
            
            const tinygltf::Value& lightsExt = model.extensions.at("KHR_lights_punctual");
            if (!lightsExt.Has("lights")) {
                std::cout << "No lights array found in extension" << std::endl;
                return;
            }
            
            const tinygltf::Value& lightsArray = lightsExt.Get("lights");
            if (!lightsArray.IsArray()) {
                std::cout << "Lights is not an array" << std::endl;
                return;
            }
            
            lights.resize(lightsArray.Size());
            
            for (size_t i = 0; i < lightsArray.Size(); i++) {
                const tinygltf::Value& lightValue = lightsArray.Get(i);
                Light& light = lights[i];
                
                // Parse color (default to white if not present)
                if (lightValue.Has("color") && lightValue.Get("color").IsArray() && lightValue.Get("color").Size() >= 3) {

                    const tinygltf::Value& colorArray = lightValue.Get("color");
                    light.color = glm::vec3(
                        colorArray.Get(0).GetNumberAsDouble(),
                        colorArray.Get(1).GetNumberAsDouble(),
                        colorArray.Get(2).GetNumberAsDouble()
                    );
                    if (length(light.color) < 0.001f) {
                        light.color = glm::vec3(1.0f); // fallback white
                    }
                } else {
                    light.color = glm::vec3(1.0f, 1.0f, 1.0f); // Default white
                }
                
                // Parse intensity (default to 1.0 if not present)
                if (lightValue.Has("intensity")) {
                    light.intensity = lightValue.Get("intensity").GetNumberAsDouble();
                    if (light.intensity < 0.001f) {
                        // Many glTF lights ship with 0 intensity; default to 1
                        light.intensity = 1.0f;
                    }
                } else {
                    light.intensity = 1.0f;
                }

                // Parse type (default to point if not present)
                if (lightValue.Has("type") && lightValue.Get("type").IsString()) {
                    std::string typeStr = lightValue.Get("type").Get<std::string>();
                    light.type = (typeStr == "directional") ? DIRECTIONAL : POINT;
                } else {
                    light.type = POINT;
                }                
            }
            
            std::cout << "Processed " << lights.size() << " lights" << std::endl;
        }


        void ProcessMeshes() {
            std::cout << "Processing meshes" << std::endl;
            meshes.clear();
            size_t totalPrimitives = 0;
            for (size_t i = 0; i < model.meshes.size(); i++) {
                totalPrimitives += model.meshes[i].primitives.size();
            }
            meshes.reserve(totalPrimitives);
            for (size_t i = 0; i < model.meshes.size(); i++) {
                const tinygltf::Mesh& gltfMesh = model.meshes[i];
                // std::cout << "Mesh " << i << ": " << gltfMesh.name << " has " << gltfMesh.primitives.size() << " primitives" << std::endl;
                for (size_t j = 0; j < gltfMesh.primitives.size(); j++) {
                    Mesh newMesh;
                    ProcessPrimitive(gltfMesh.primitives[j], newMesh);
                    // std::cout << "  Primitive " << j << ": vertices=" << newMesh.vertices.size() << ", indices=" << newMesh.indices.size() << ", materialIndex=" << newMesh.materialIndex << std::endl;
                    meshes.push_back(std::move(newMesh));
                }
            }
            std::cout << "Created " << meshes.size() << " mesh objects from primitives" << std::endl;
        }

        glm::mat4 GetTransformMatrix(const tinygltf::Node &node) {
            glm::mat4 transform = glm::mat4(1.0f);

            glm::vec3 translation = glm::vec3(0.0f);
            glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 scale = glm::vec3(1.0f);

            if (node.translation.size() == 3) {
                translation = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
            }

            if (node.rotation.size() == 4) {
                rotation = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
            }

            if (node.scale.size() == 3) {
                scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
            }

            transform = glm::translate(glm::mat4(1.0f), translation) *
                       glm::mat4_cast(rotation) *
                       glm::scale(glm::mat4(1.0f), scale);

            return transform;
        }

        void CalculateWorldTransform(int nodeIndex, const glm::mat4& parentTransform) {
            Node& node = nodes[nodeIndex];
            node.worldTransform = parentTransform * node.transform;
        }


        void ProcessNodes() {
            nodes.resize(model.nodes.size());

            for (size_t i = 0; i < model.nodes.size(); i++) {
                const tinygltf::Node& gltfNode = model.nodes[i];
                Node& node = nodes[i];

                // Get transform matrix
                node.transform = GetTransformMatrix(gltfNode);

                // check light source
                if (gltfNode.extensions.find("KHR_lights_punctual") != gltfNode.extensions.end()) {
                    const tinygltf::Value& lightsExt = gltfNode.extensions.at("KHR_lights_punctual");
                    if (lightsExt.Has("light")) {
                        const tinygltf::Value& lightIndex = lightsExt.Get("light");
                        if (lightIndex.IsNumber()) {
                            int lightIdx = lightIndex.GetNumberAsInt();
                            if (lightIdx >= 0 && lightIdx < lights.size()) {
                                node.light = lights[lightIdx];
                                node.hasLight = true;
                                std::cout << "Node " << i << " (" << gltfNode.name << ") has light " << lightIdx << std::endl;
                            }
                        }
                    }
                }

                // Store mesh indices - need to map to flattened primitive array
                if (gltfNode.mesh >= 0) {
                    // Calculate the starting index for this mesh's primitives
                    int primitiveStartIndex = 0;
                    for (int m = 0; m < gltfNode.mesh; m++) {
                        primitiveStartIndex += model.meshes[m].primitives.size();
                    }
                    
                    // Add all primitive indices for this mesh
                    const tinygltf::Mesh& gltfMesh = model.meshes[gltfNode.mesh];
                    for (size_t p = 0; p < gltfMesh.primitives.size(); p++) {
                        node.meshIndices.push_back(primitiveStartIndex + p);
                    }
                }

                // Store children
                if (gltfNode.children.size() > 0) {
                    node.children = gltfNode.children;
                }
            }
        }

        // Recursive rendering with parent transform
        void RenderNodes(int nodeIndex,
                         Shader shaderProgram,
                         const glm::mat4& view,
                         const glm::mat4& projection,
                         const glm::mat4& parentTransform) {
            const Node &node = nodes[nodeIndex];

            // Compose local with parent
            glm::mat4 model = parentTransform * node.transform;
            glm::mat4 MVP   = projection * view * model;
            shaderProgram.setMat4("MVP", MVP);
            shaderProgram.setMat4("model", model);

            // Set camera position for specular lighting - handled in main loop

            // Render all meshes referenced by this node
            for (int meshIndex : node.meshIndices) {
                const Mesh &mesh = meshes[meshIndex];

                bool enableAlphaBlend = false; // scope-wide flag so we can restore GL state after draw
                if (mesh.materialIndex >= 0) {
                    const Material &material = materials[mesh.materialIndex];
                    
                    // Check if depth mask should be disabled for transparent materials
                    enableAlphaBlend = false;
                    std::string matName = "";
                    if (mesh.materialIndex < this->model.materials.size()) {
                        const tinygltf::Material& gltfMaterial = this->model.materials[mesh.materialIndex];
                        if (gltfMaterial.alphaMode == "BLEND") {
                            enableAlphaBlend = true;
                        }
                        if (gltfMaterial.name.size()) matName = gltfMaterial.name;
                    }
                    if (enableAlphaBlend) {
                        glDepthMask(GL_FALSE); // Don't write to depth buffer for transparent objects
                    }
                    if (material.baseColorTexture >= 0) {
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, textures[material.baseColorTexture].textureID);
                        shaderProgram.setInt("albedoMap", 0);
                        
                    } else {
                        // Bind a default white texture for albedo
                        static unsigned int defaultWhiteTex = 0;
                        if (defaultWhiteTex == 0) {
                            unsigned char whitePixel[4] = {255, 255, 255, 255};
                            glGenTextures(1, &defaultWhiteTex);
                            glBindTexture(GL_TEXTURE_2D, defaultWhiteTex);
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        }
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, defaultWhiteTex);
                        shaderProgram.setInt("albedoMap", 0);
                    }

                    if (material.normalTexture >= 0) {
                        glActiveTexture(GL_TEXTURE1);
                        glBindTexture(GL_TEXTURE_2D, textures[material.normalTexture].textureID);
                        shaderProgram.setInt("normalMap", 1);
                    } else {
                       
                        // Bind a default normal map (flat normal - pointing up)
                        static unsigned int defaultNormalTex = 0;
                        if (defaultNormalTex == 0) {
                            unsigned char normalPixel[4] = {127, 127, 255, 255}; // (0.5, 0.5, 1.0) in [0,255]
                            glGenTextures(1, &defaultNormalTex);
                            glBindTexture(GL_TEXTURE_2D, defaultNormalTex);
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, normalPixel);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        }
                        glActiveTexture(GL_TEXTURE1);
                        glBindTexture(GL_TEXTURE_2D, defaultNormalTex);
                        shaderProgram.setInt("normalMap", 1);
                    }

                    if (material.metallicRoughnessTexture >= 0) {
                        glActiveTexture(GL_TEXTURE4);  // Changed from GL_TEXTURE3 to GL_TEXTURE4 to avoid shadow map conflict
                        glBindTexture(GL_TEXTURE_2D, textures[material.metallicRoughnessTexture].textureID);
                        shaderProgram.setInt("metallicRoughnessMap", 4);
                    } else {
                        // Bind a default metallic-roughness texture (no metallic, medium roughness)
                        static unsigned int defaultMetallicRoughnessTex = 0;
                        if (defaultMetallicRoughnessTex == 0) {
                            unsigned char mrPixel[4] = {0, 127, 0, 255}; // metallic=0, roughness=0.5
                            glGenTextures(1, &defaultMetallicRoughnessTex);
                            glBindTexture(GL_TEXTURE_2D, defaultMetallicRoughnessTex);
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, mrPixel);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        }
                        glActiveTexture(GL_TEXTURE4);
                        glBindTexture(GL_TEXTURE_2D, defaultMetallicRoughnessTex);
                        shaderProgram.setInt("metallicRoughnessMap", 4);
                    }
                    
                    // Bind occlusion (AO) texture or default white
                    if (material.occlusionTexture >= 0) {
                        glActiveTexture(GL_TEXTURE3);
                        glBindTexture(GL_TEXTURE_2D, textures[material.occlusionTexture].textureID);
                        shaderProgram.setInt("aoMap", 3);
                    } else {
                        static unsigned int defaultAOTex = 0;
                        if (defaultAOTex == 0) {
                            unsigned char whitePixel[4] = {255, 255, 255, 255};
                            glGenTextures(1, &defaultAOTex);
                            glBindTexture(GL_TEXTURE_2D, defaultAOTex);
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        }
                        glActiveTexture(GL_TEXTURE3);
                        glBindTexture(GL_TEXTURE_2D, defaultAOTex);
                        shaderProgram.setInt("aoMap", 3);
                    }
                    
                    // Set material properties
                    shaderProgram.setFloat("metallicFactor", material.metallicFactor);
                    shaderProgram.setFloat("roughnessFactor", material.roughnessFactor);
                    
                    shaderProgram.setVec4("baseColorFactor", material.baseColorFactor);
                    

                }

                glBindVertexArray(mesh.VAO);
                glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
                // Restore depth mask if we disabled it for transparent materials
                if (enableAlphaBlend) {
                    glDepthMask(GL_TRUE);
                }
            }

            // TODO: if the model had child nodes, we would recurse here
            for (int childIndex : node.children) {
                RenderNodes(childIndex, shaderProgram, view, projection, model);
            }
        }

    public:
        // Retrieve the first directional light found in the scene.
        // Returns true if one exists, false otherwise.
        bool getFirstDirectionalLight(glm::vec3 &outPosition, glm::vec3 &outDirection) const {
            for (const Node &node : nodes) {
                if (!node.hasLight) continue;
                if (node.light.type != DIRECTIONAL) continue;
                // If the author left intensity at 0 treat it as disabled

                glm::mat4 model = node.transform;
                outPosition  = glm::vec3(model[3]);
                outDirection = glm::normalize(glm::mat3(model) * glm::vec3(0.0f, 0.0f, -1.0f));
                return true;
            }
            return false; // none found
        }

        // Retrieve the first directional light with its full transform matrix
        bool getFirstDirectionalLightTransform(glm::mat4 &outTransform) const {
            for (const Node &node : nodes) {
                if (!node.hasLight) continue;
                if (node.light.type != DIRECTIONAL) continue;
                
                outTransform = node.transform;
                return true;
            }
            return false; // none found
        }

        void setupLighting(Shader shaderProgram) {
            // Find first light node and get its transform
            std::vector<Node> lightNodes;
            for (int i = 0; i < nodes.size(); i++) {
                const Node &node = nodes[i];

                if (node.hasLight) {
                    lightNodes.push_back(node);
                }
            }

            // Initialize all lights to inactive first
            const int MAX_LIGHTS = 2; // Must match NR_LIGHTS in shader
            for (int i = 0; i < MAX_LIGHTS; i++) {
                shaderProgram.setBool("light[" + std::to_string(i) + "].isPointLight", false);
                shaderProgram.setVec3("light[" + std::to_string(i) + "].color", glm::vec3(0.0f));
                shaderProgram.setFloat("light[" + std::to_string(i) + "].intensity", 0.0f);
            }

            // Set up actual lights (limit to MAX_LIGHTS)
            int numLights = std::min((int)lightNodes.size(), MAX_LIGHTS);
            int directionalLightCount = 0;
            int pointLightCount = 0;

            // Only process directional lights
            for (int i = 0; i < numLights; i++) {
                const Node &node = lightNodes[i];
                const Light &light = node.light;
                glm::mat4 model = node.transform; // Use node's transform matrix
                
                if (light.type == DIRECTIONAL) {
                    std::cout << "Using directional light" << std::endl;
                    
                    // DIRECTIONAL light - calculate direction from node's rotation (forward is -Z)
                    glm::vec3 lightDir = glm::mat3(model) * glm::vec3(0.0f, 0.0f, -1.0f);
                    lightDir = glm::normalize(lightDir);

                    std::cout << "Light Color: " << light.color.x << ", " << light.color.y << ", " << light.color.z << std::endl;
                    std::cout << "Light Direction: " << lightDir.x << ", " << lightDir.y << ", " << lightDir.z << std::endl;
                    
                    shaderProgram.setVec3("dirLight[" + std::to_string(directionalLightCount)+ "].direction", lightDir);
                    shaderProgram.setVec3("dirLight[" + std::to_string(directionalLightCount) + "].color", light.color);
                    shaderProgram.setFloat("dirLight[" + std::to_string(directionalLightCount) + "].intensity", light.intensity);
                    shaderProgram.setBool("dirLight[" + std::to_string(directionalLightCount) + "].isPointLight", false);

                    glm::vec3 lightPos = glm::vec3(model[3]);
                    shaderProgram.setVec3("dirLight["+ std::to_string(directionalLightCount) + "].lightPos", lightPos); // unused for directional

                    directionalLightCount++;
                    
                    // Break after finding first directional light
                    if (directionalLightCount >= 1) {
                        break;
                    }
                }
            }
        }

        


        void Render(Shader shaderProgram, const glm::mat4& view, const glm::mat4& projection) {
            glm::mat4 identity = glm::mat4(1.0f);
            for (const auto &scene : model.scenes) {
                int lightIndex = 0; // only render first light
                for (const auto &nodeIndex : scene.nodes) {
                    RenderNodes(nodeIndex, shaderProgram, view, projection, identity);

                    // Debug: render a cube at light positions if enabled and show first light
                    if (showLightCubes && nodes[nodeIndex].hasLight && lightIndex < 2) {
                        // render only one light cube
                        glm::mat4 lightTransform = nodes[nodeIndex].transform;
                        renderLightCube(lightTransform, shaderProgram, view, projection, nodes[nodeIndex].light.color);
                        lightIndex++;
                    }
                }
            }
        }
};
#endif