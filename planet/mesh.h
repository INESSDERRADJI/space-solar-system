#ifndef MESH_H
#define MESH_H

#include <GL/glew.h>

#include <glm/glm.hpp>

#include <cstddef>   // offsetof
#include <string>
#include <vector>

#include "shader.h"

#define MAX_BONE_INFLUENCE 4

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;

    glm::vec3 Tangent;
    glm::vec3 Bitangent;

    int   m_BoneIDs[MAX_BONE_INFLUENCE];
    float m_Weights[MAX_BONE_INFLUENCE];
};

struct Texture {
    unsigned int id;
    std::string type;   
    std::string path;
};

class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    unsigned int VAO = 0;

    Mesh(const std::vector<Vertex>& vertices,
        const std::vector<unsigned int>& indices,
        const std::vector<Texture>& textures)
        : vertices(vertices), indices(indices), textures(textures) {
        setupMesh();
    }

    void Draw(Shader& shader) {
        unsigned int diffuseNr = 1;
        unsigned int specularNr = 1;
        unsigned int normalNr = 1;
        unsigned int heightNr = 1;

        for (unsigned int i = 0; i < textures.size(); i++) {
            glActiveTexture(GL_TEXTURE0 + i);

            std::string number;
            std::string name = textures[i].type;
            if (name == "texture_diffuse")      number = std::to_string(diffuseNr++);
            else if (name == "texture_specular") number = std::to_string(specularNr++);
            else if (name == "texture_normal")   number = std::to_string(normalNr++);
            else if (name == "texture_height")   number = std::to_string(heightNr++);

            glUniform1i(glGetUniformLocation(shader.ID, (name + number).c_str()), (int)i);
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, (unsigned int)indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glActiveTexture(GL_TEXTURE0);
    }

    void Draw2(Shader& shader,
        const std::string& nightUniform, unsigned int nightID,
        const std::string& cloudUniform, unsigned int cloudID,
        GLfloat time) {

        unsigned int diffuseNr = 1;
        unsigned int specularNr = 1;
        unsigned int normalNr = 1;
        unsigned int heightNr = 1;

        for (unsigned int i = 0; i < textures.size(); i++) {
            glActiveTexture(GL_TEXTURE0 + i);

            std::string number;
            std::string name = textures[i].type;
            if (name == "texture_diffuse")      number = std::to_string(diffuseNr++);
            else if (name == "texture_specular") number = std::to_string(specularNr++);
            else if (name == "texture_normal")   number = std::to_string(normalNr++);
            else if (name == "texture_height")   number = std::to_string(heightNr++);

            glUniform1i(glGetUniformLocation(shader.ID, (name + number).c_str()), (int)i);
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }

        unsigned int baseUnit = (unsigned int)textures.size();
        unsigned int nightUnit = baseUnit;
        unsigned int cloudUnit = baseUnit + 1;

        glActiveTexture(GL_TEXTURE0 + nightUnit);
        glBindTexture(GL_TEXTURE_2D, nightID);
        glUniform1i(glGetUniformLocation(shader.ID, nightUniform.c_str()), (int)nightUnit);

        glActiveTexture(GL_TEXTURE0 + cloudUnit);
        glBindTexture(GL_TEXTURE_2D, cloudID);
        glUniform1i(glGetUniformLocation(shader.ID, cloudUniform.c_str()), (int)cloudUnit);

        glUniform1f(glGetUniformLocation(shader.ID, "time"), time);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, (unsigned int)indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glActiveTexture(GL_TEXTURE0);
    }

private:
    unsigned int VBO = 0, EBO = 0;

    void setupMesh() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
            vertices.size() * sizeof(Vertex),
            vertices.data(),
            GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            indices.size() * sizeof(unsigned int),
            indices.data(),
            GL_STATIC_DRAW);

        // layout(location = 0) position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));

        // layout(location = 1) normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

        // layout(location = 2) texcoords
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

        
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));

        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Bitangent));

        glEnableVertexAttribArray(5);
        glVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, m_BoneIDs));

        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_Weights));

        glBindVertexArray(0);
    }
};

#endif
