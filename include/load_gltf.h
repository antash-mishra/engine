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
#include <cassert>
#include <utility>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLT_BUFFER_OFFSET(i) ((char *)nullptr + (i))



// --------------------------------------------------------------------------------
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
    glm::vec3 tangent;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    int materialIndex;
    unsigned int VAO, VBO, EBO;
};

struct Material {
    glm::vec4 baseColorFactor;
    int baseColorTexture;
    int normalTexture;
    int metallicRoughnessTexture;
    float metallicFactor;
    float roughnessFactor;
};


struct Texture {
    unsigned int textureID;
    int samplerIndex;
};

struct Node {
    std::vector<int> meshIndices;
    // std::vector<int> children;
    glm::mat4 transform;
    glm::mat4 worldTransform;
};



class GLTFLoader {
    public:
        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        std::vector<Texture> textures;
        std::vector<Node> nodes;
        tinygltf::Model model;

        // Load a glTF/GLB file into the provided tinygltf::Model.
        // Returns true on success.
        bool loadModel(const std::string &filename) {
            std::cout << "loadModel" << std::endl;
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
            std::cout << "Loaded glTF: " << filename << std::endl;

            ProcessTextures();
            ProcessMaterials();
            ProcessMeshes();
            ProcessNodes();
            return true;
        }

    private:

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

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));

            glBindVertexArray(0);
        }

        void ProcessPrimitive(const tinygltf::Primitive& primitive, Mesh& mesh) {
            std::vector<glm::vec3> positions;
            std::vector<glm::vec3> normals;
            std::vector<glm::vec2> texCoords;
            std::vector<glm::vec3> tangents;

            // Extract position
            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("POSITION")];
                positions = GetVec3Data(accessor);
            }

            // Extract texCoords
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                texCoords = GetVec2Data(accessor);
            }

            // Extract Normal
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
                normals = GetVec3Data(accessor);
            }

            // Extract Tangent
            if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TANGENT")];
                tangents = GetVec3Data(accessor);
            }

            // Build vertex Array
            size_t vertexCount = positions.size();
            mesh.vertices.resize(vertexCount);

            for (size_t v = 0; v < vertexCount; v++) {
                mesh.vertices[v].position = positions[v];
                mesh.vertices[v].normal = (v < normals.size()) ? normals[v] : glm::vec3(0.0f);
                mesh.vertices[v].texCoords = (v < texCoords.size()) ? texCoords[v] : glm::vec2(0.0f);
                mesh.vertices[v].tangent = (v < tangents.size()) ? tangents[v] : glm::vec3(0.0f);
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
            std::cout << "Processing textures" << std::endl;
            textures.resize(model.textures.size());

            for (size_t t = 0; t < model.textures.size(); t++) {
                const tinygltf::Texture& gltfTexture = model.textures[t];

                if (gltfTexture.source >= 0) {
                    const tinygltf::Image& gltfImage = model.images[gltfTexture.source];

                    // create Texture
                    GLuint texID;
                    glGenTextures(1, &texID);
                    glBindTexture(GL_TEXTURE_2D, texID);

                    // Determine format based on image component
                    GLenum format;
                    if (gltfImage.component == 1) format = GL_RED;
                    else if (gltfImage.component == 2) format = GL_RG;
                    else if (gltfImage.component == 3) format = GL_RGB;
                    else if (gltfImage.component == 4) format = GL_RGBA;

                    glTexImage2D(GL_TEXTURE_2D, 0, format, gltfImage.width, gltfImage.height, 0, format, GL_UNSIGNED_BYTE,  gltfImage.image.data());

                    // Set texture parameters based on sampler
                    if (gltfTexture.sampler >= 0) {
                        const tinygltf::Sampler& sampler = model.samplers[gltfTexture.sampler];
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampler.minFilter);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampler.magFilter);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler.wrapS);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler.wrapT);
                    } else {
                        // Default parameters
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    }

                    glGenerateMipmap(GL_TEXTURE_2D);

                    textures[t].textureID = texID;
                    textures[t].samplerIndex = gltfTexture.sampler;
                }
            }
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

                material.metallicFactor = gltfMaterial.pbrMetallicRoughness.metallicFactor;
                material.roughnessFactor = gltfMaterial.pbrMetallicRoughness.roughnessFactor;

                // Texture indices
                material.baseColorTexture  = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
                material.normalTexture = gltfMaterial.normalTexture.index;
                material.metallicRoughnessTexture = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
            }
        }


        void ProcessMeshes() {
            std::cout << "Processing meshes" << std::endl;
            // Don't resize based on model.meshes.size() since we need one Mesh per primitive
            meshes.clear();
            
            for (size_t i = 0; i < model.meshes.size(); i++) {
                const tinygltf::Mesh& gltfMesh = model.meshes[i];

                // Process each primitive in the mesh as a separate Mesh object
                for (size_t j = 0; j < gltfMesh.primitives.size(); j++) {
                    Mesh newMesh;
                    ProcessPrimitive(gltfMesh.primitives[j], newMesh);
                    meshes.push_back(newMesh);
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

            // Render all meshes referenced by this node
            for (int meshIndex : node.meshIndices) {
                const Mesh &mesh = meshes[meshIndex];

                // Bind material (only base color for now)
                if (mesh.materialIndex >= 0) {
                    const Material &material = materials[mesh.materialIndex];
                    if (material.baseColorTexture >= 0) {
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, textures[material.baseColorTexture].textureID);
                        shaderProgram.setInt("baseColorTexture", 0);
                    }
                }

                glBindVertexArray(mesh.VAO);
                glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
            }

            // TODO: if the model had child nodes, we would recurse here
        }

    public:
        void Render(Shader shaderProgram, const glm::mat4& view, const glm::mat4& projection) {
            glm::mat4 identity = glm::mat4(1.0f);
            for (const auto &scene : model.scenes) {
                for (const auto &nodeIndex : scene.nodes) {
                    RenderNodes(nodeIndex, shaderProgram, view, projection, identity);
                }
            }
        }
};
#endif