#include "model.h"
#include "mesh.h"
#include "shader.h"
#include "stb_image.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <cstring>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

unsigned int TextureFromFile(const char *path, const std::string &directory);

Model::Model(char *path) {
  loadModel(path);
}

void Model::Draw(Shader &shader){
  for (unsigned int i=0; i< meshes.size(); i++) {
    meshes[i].Draw(shader);
  }
}

void Model::loadModel(std::string path){
  Assimp::Importer importer;
  const aiScene *scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode){
    std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
    return;
  }
  
  directory = path.substr(0, path.find_last_of("/"));
  processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode *node, const aiScene *scene) {
  // process all nodes mesh
  for(unsigned int i=0; i < node->mNumMeshes; i++) {
    aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
    meshes.push_back(processMesh(mesh, scene));
  }

  // do the same for each of nodes children
  for (unsigned int i=0; i< node->mNumChildren; i++) {
    processNode(node->mChildren[i], scene);
  }
}

Mesh Model::processMesh(aiMesh *mesh, const aiScene *scene) {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  std::vector<Texture> textures;

  // process vertices from assimp to openGL
  for (unsigned int i=0; i< mesh->mNumVertices; i++) {
    Vertex vertex;
    glm::vec3 vector;
    vector.x = mesh->mVertices[i].x;
    vector.y = mesh->mVertices[i].y;
    vector.z = mesh->mVertices[i].z;
    vertex.Position = vector;

    vector.x = mesh->mNormals[i].x;
    vector.y = mesh->mNormals[i].y;
    vector.z = mesh->mNormals[i].z;

    if(mesh->mTextureCoords[0]){
      glm::vec2 vec;
      vec.x = mesh->mTextureCoords[0][i].x;
      vec.y = mesh->mTextureCoords[0][i].y;
      vertex.TexCoords = vec;
    }
    else {
      vertex.TexCoords = glm::vec2(0.0, 0.0);
    }
    vertices.push_back(vertex);
  }

  // process indices from assimp to openGL format
  for (unsigned int i=0; i<mesh->mNumFaces; i++) {
    aiFace face = mesh->mFaces[i];
    for (unsigned int j=0; j< face.mNumIndices; j++){
      indices.push_back(face.mIndices[j]);
    }
  }

  // process material from assimp data struct to our defined OpenGL data structure
  if(mesh->mMaterialIndex >= 0){
    aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
    // load diffuse map texture
    std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE,"texture_diffuse");
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    // load specular map texture
    std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
  }

  return Mesh(vertices, indices, textures);
}

std::vector<Texture> Model::loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName) {
  // loads and generates the texture and store info in Vertex struct
  std::vector<Texture> textures;
  for (unsigned int i=0; i<mat->GetTextureCount(type); i++){
    aiString str;
    mat->GetTexture(type, i, &str);
    bool skip = false;

    for (unsigned int j=0; j<textures_loaded.size(); j++) {
      if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
        textures.push_back(textures_loaded[j]);
        skip=true;
        break;
      }
    }
    if (!skip) {
      Texture texture;
      texture.id = TextureFromFile(str.C_Str(), this->directory);
      texture.type = typeName;
      texture.path = str.C_Str();
      textures.push_back(texture);
      textures_loaded.push_back(texture);
    }
  }
  return textures; 
}

unsigned int TextureFromFile(const char *path, const std::string &directory){
  std::string fileName = std::string(path);
  fileName = directory + '/' + fileName;

  unsigned int textureID;
  glGenTextures(1, &textureID);

  int width, height, nrComponents;
  
  // Load image using stb_image
  stbi_set_flip_vertically_on_load(true);
    
  unsigned char* data = stbi_load(fileName.c_str(), &width, &height, &nrComponents, 0);
    
  if (data) {
    GLenum format;
    
    // Set format based on channels
    if (nrComponents == 1)
      format = GL_RED;
    else if (nrComponents == 3)
      format = GL_RGB;
    else if (nrComponents == 4)
      format = GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
  } else {
    std::cout << "Texture failed to load: " << path << std::endl;
    stbi_image_free(data);
  }

  return textureID;
}




