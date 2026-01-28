#include "asteroid_belt.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <random>

AsteroidBelt::AsteroidBelt(Model* model, unsigned int amount)
    : rockModel(model), amount(amount), instanceVBO(0)
{
    setupMatrices();
}

AsteroidBelt::~AsteroidBelt()
{
    if (instanceVBO != 0)
        glDeleteBuffers(1, &instanceVBO);
}

void AsteroidBelt::setupMatrices()
{
    std::vector<glm::mat4> modelMatrices(amount);

    // Random engine (seeded)
    std::default_random_engine generator(std::random_device{}());
    std::uniform_real_distribution<float> offsetDist(-15.0f, 15.0f);
    std::uniform_real_distribution<float> scaleDist(0.05f, 0.25f);
    std::uniform_real_distribution<float> rotDist(0.0f, glm::two_pi<float>());

    float radius = 50.0f;

    for (unsigned int i = 0; i < amount; ++i)
    {
        glm::mat4 model(1.0f);

        // Angle en radians (IMPORTANT)
        float angle = (float)i / (float)amount * glm::two_pi<float>();

        float x = std::sin(angle) * radius + offsetDist(generator);
        float y = offsetDist(generator) * 0.4f;
        float z = std::cos(angle) * radius + offsetDist(generator);

        model = glm::translate(model, glm::vec3(x, y, z));

        float scale = scaleDist(generator);
        model = glm::scale(model, glm::vec3(scale));

        float rotAngle = rotDist(generator);
        model = glm::rotate(
            model,
            rotAngle,
            glm::normalize(glm::vec3(0.4f, 0.6f, 0.8f))
        );

        modelMatrices[i] = model;
    }

    // Instance buffer
    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        amount * sizeof(glm::mat4),
        modelMatrices.data(),
        GL_STATIC_DRAW
    );

    // Configure instance attributes for each mesh
    for (unsigned int i = 0; i < rockModel->meshes.size(); ++i)
    {
        unsigned int VAO = rockModel->meshes[i].VAO;
        glBindVertexArray(VAO);

        std::size_t vec4Size = sizeof(glm::vec4);

        // Locations 7,8,9,10 : mat4 instanceMatrix
        glEnableVertexAttribArray(7);
        glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)0);

        glEnableVertexAttribArray(8);
        glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(vec4Size));

        glEnableVertexAttribArray(9);
        glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(2 * vec4Size));

        glEnableVertexAttribArray(10);
        glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(3 * vec4Size));

        glVertexAttribDivisor(7, 1);
        glVertexAttribDivisor(8, 1);
        glVertexAttribDivisor(9, 1);
        glVertexAttribDivisor(10, 1);

        glBindVertexArray(0);
    }
}

void AsteroidBelt::Draw(Shader& shader,
                        const glm::mat4& view,
                        const glm::mat4& projection)
{
    shader.use();
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);

    for (unsigned int i = 0; i < rockModel->meshes.size(); ++i)
    {
        // Bind textures
        for (unsigned int j = 0; j < rockModel->meshes[i].textures.size(); ++j)
        {
            glActiveTexture(GL_TEXTURE0 + j);
            glBindTexture(
                GL_TEXTURE_2D,
                rockModel->meshes[i].textures[j].id
            );
        }

        glBindVertexArray(rockModel->meshes[i].VAO);

        glDrawElementsInstanced(
            GL_TRIANGLES,
            static_cast<unsigned int>(rockModel->meshes[i].indices.size()),
            GL_UNSIGNED_INT,
            0,
            amount
        );

        glBindVertexArray(0);
    }

    glActiveTexture(GL_TEXTURE0);
}
