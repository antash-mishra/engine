#ifndef TERRAIN_GENERATION_H
#define TERRAIN_GENERATION_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <vector>
#include "shader.h"

// Vertex struct for terrain generation
// NOTE: std430 layout aligns vec3 to 16 bytes, so vec3 -> 12 bytes + 4 padding.
// To keep CPU and GPU memory layouts identical, we add explicit padding fields.
// stride = 48 bytes (multiple of 16).
struct Vertex {
    glm::vec3 position; float _padPosition;   // 16 bytes
    glm::vec2 texCoord; glm::vec2 _padTex;    // 16 bytes (offset 16)
    glm::vec3 normal;   float _padNormal;     // 16 bytes (offset 32)
};

class TerrainGenerator {
    private:
        Shader computeShader;
        unsigned int vertexBuffer;
        unsigned int vertexArray;
        unsigned int indexBuffer; // EBO
        unsigned int terrainWidth;
        unsigned int terrainHeight;
        unsigned int gridSize;
        int numIndices; // Store number of indices for rendering
        

    public:
        TerrainGenerator(const char* computerShaderPath, int gridSize = 256, int width = 505, int height = 505): 
        computeShader(computerShaderPath), gridSize(gridSize), terrainWidth(width), terrainHeight(height) {
            setupBuffers();
        }

        void generateTerrain(unsigned int heightMapTexture) {
            computeShader.use();
            computeShader.setInt("gridSize", gridSize);
            computeShader.setInt("terrainWidth", terrainWidth);
            computeShader.setInt("terrainHeight", terrainHeight);
            computeShader.setFloat("heightScale", 64.0f);
            computeShader.setFloat("heightShift", 16.0f);

            // Bind heightmap
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, heightMapTexture);
            computeShader.setInt("heightMap", 0);

            // Bind vertex buffer
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, vertexBuffer);
            
            // Dispatch compute shader
            int numGroups = (gridSize + 15) / 16;
            glDispatchCompute(numGroups, numGroups, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }

        void render() {
            // Bind vertex array
            glBindVertexArray(vertexArray);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            // Draw terrain using indices
            glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, 0);
        }
    
    private:
        void setupBuffers() {
            // Create vertex buffer
            glGenBuffers(1, &vertexBuffer);
            // Allocate as SSBO for compute
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexBuffer);
            glBufferData(GL_SHADER_STORAGE_BUFFER, gridSize * gridSize * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);

            // Create vertex array
            glGenVertexArrays(1, &vertexArray);
            glBindVertexArray(vertexArray);

            // Bind the same buffer as GL_ARRAY_BUFFER for rendering
            glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

            // Set up vertex attributes
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

            // Generate index buffer for grid mesh
            std::vector<unsigned int> indices;
            for (int y = 0; y < gridSize - 1; ++y) {
                for (int x = 0; x < gridSize - 1; ++x) {
                    unsigned int topLeft = y * gridSize + x;
                    unsigned int topRight = topLeft + 1;
                    unsigned int bottomLeft = (y + 1) * gridSize + x;
                    unsigned int bottomRight = bottomLeft + 1;
                    // First triangle
                    indices.push_back(topLeft);
                    indices.push_back(bottomLeft);
                    indices.push_back(topRight);
                    // Second triangle
                    indices.push_back(topRight);
                    indices.push_back(bottomLeft);
                    indices.push_back(bottomRight);
                }
            }
            numIndices = static_cast<int>(indices.size());
            glGenBuffers(1, &indexBuffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        }

        void setupVertexAttributes() {}

};

#endif