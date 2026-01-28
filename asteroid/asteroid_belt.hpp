#ifndef ASTEROID_BELT_HPP
#define ASTEROID_BELT_HPP

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "model.h" // Assure-toi que ta classe Model est bien accessible ici
#include "shader.h"

class AsteroidBelt {
public:
    // model : le modèle .obj chargé / amount : nombre d'astéroïdes
    AsteroidBelt(Model* model, unsigned int amount = 2000);
    ~AsteroidBelt();

    void Draw(Shader& shader, const glm::mat4& view, const glm::mat4& projection);

private:
    Model* rockModel;
    unsigned int amount;
    unsigned int instanceVBO;

    void setupMatrices();
};

#endif