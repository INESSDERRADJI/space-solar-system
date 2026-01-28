#ifndef MODEL_H
#define MODEL_H

#include <GL/glew.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stb_image.h>

#include <cstring>   // std::strcmp
#include <iostream>
#include <string>
#include <vector>

#include "mesh.h"
#include "shader.h"

inline unsigned int TextureFromFile(const char* path,
    const std::string& directory,
    bool gamma = false);

class Model {
public:
    std::vector<Texture> textures_loaded;
    std::vector<Mesh> meshes;
    std::string directory;
    bool gammaCorrection = false;

    Model(const std::string& path, bool gamma = false) : gammaCorrection(gamma) {
        loadModel(path);
    }

    void Draw(Shader& shader) {
        for (auto& m : meshes) m.Draw(shader);
    }

    // Terre : diffuse + night + clouds + time
    void Draw2(Shader& shader,
        const std::string& nightName, unsigned int nightID,
        const std::string& cloudName, unsigned int cloudID,
        GLfloat time) {
        for (auto& m : meshes) m.Draw2(shader, nightName, nightID, cloudName, cloudID, time);
    }

private:
    void loadModel(const std::string& path) {
        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_FlipUVs |
            aiProcess_CalcTangentSpace
        );

        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
            std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
            return;
        }

        // directory
        size_t slash = path.find_last_of("/\\");
        directory = (slash == std::string::npos) ? "." : path.substr(0, slash);

        processNode(scene->mRootNode, scene);
    }

    void processNode(aiNode* node, const aiScene* scene) {
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i], scene);
        }
    }

    Mesh processMesh(aiMesh* mesh, const aiScene* scene) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture> textures;

        vertices.reserve(mesh->mNumVertices);

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex{};

            // init bones/weights 
            for (int b = 0; b < MAX_BONE_INFLUENCE; b++) {
                vertex.m_BoneIDs[b] = -1;
                vertex.m_Weights[b] = 0.0f;
            }

            // positions
            vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

            // normals
            if (mesh->HasNormals()) {
                vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            }
            else {
                vertex.Normal = glm::vec3(0.0f);
            }

            // texcoords + tangent/bitangent
            if (mesh->mTextureCoords[0]) {
                vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);

                if (mesh->mTangents) {
                    vertex.Tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
                }
                else {
                    vertex.Tangent = glm::vec3(0.0f);
                }

                if (mesh->mBitangents) {
                    vertex.Bitangent = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
                }
                else {
                    vertex.Bitangent = glm::vec3(0.0f);
                }
            }
            else {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
                vertex.Tangent = glm::vec3(0.0f);
                vertex.Bitangent = glm::vec3(0.0f);
            }

            vertices.push_back(vertex);
        }

        // indices
        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        // materials/textures
        if (mesh->mMaterialIndex >= 0) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

            // diffuse
            auto diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
            textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

            // specular
            auto specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
            textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

            // normal 
            auto normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
            textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

            // height 
            auto heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
            textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
        }

        return Mesh(vertices, indices, textures);
    }

    std::vector<Texture> loadMaterialTextures(aiMaterial* mat,
        aiTextureType type,
        const std::string& typeName) {
        std::vector<Texture> textures;

        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
            aiString str;
            mat->GetTexture(type, i, &str);

            bool skip = false;
            for (unsigned int j = 0; j < textures_loaded.size(); j++) {
                if (std::strcmp(textures_loaded[j].path.c_str(), str.C_Str()) == 0) {
                    textures.push_back(textures_loaded[j]);
                    skip = true;
                    break;
                }
            }

            if (!skip) {
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), directory, gammaCorrection);
                texture.type = typeName;
                texture.path = str.C_Str();

                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
        }

        return textures;
    }
};

// TextureFromFile 
inline unsigned int TextureFromFile(const char* path,
    const std::string& directory,
    bool gamma) {
    std::string filename(path);

    if (!directory.empty() && directory != ".") {
        
#ifdef _WIN32
        bool isAbs = (filename.size() > 2 && std::isalpha((unsigned char)filename[0]) && filename[1] == ':');
#else
        bool isAbs = (!filename.empty() && filename[0] == '/');
#endif
        if (!isAbs) filename = directory + "/" + filename;
    }
    else {
        // directory="." -> juste "./" + filename si pas déjà relatif correct
        // (optionnel, ça marche même sans)
        // filename = "./" + filename;
    }

    unsigned int textureID = 0;
    glGenTextures(1, &textureID);

    int width = 0, height = 0, nrComponents = 0;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);

    if (!data) {
        std::cout << "Texture failed to load: " << filename << std::endl;
        stbi_image_free(data);
        return textureID;
    }

    GLenum dataFormat = GL_RGB;
    GLenum internalFormat = GL_RGB;

    if (nrComponents == 1) {
        dataFormat = internalFormat = GL_RED;
    }
    else if (nrComponents == 3) {
        dataFormat = GL_RGB;
        internalFormat = gamma ? GL_SRGB : GL_RGB;
    }
    else if (nrComponents == 4) {
        dataFormat = GL_RGBA;
        internalFormat = gamma ? GL_SRGB_ALPHA : GL_RGBA;
    }

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return textureID;
}

#endif 
