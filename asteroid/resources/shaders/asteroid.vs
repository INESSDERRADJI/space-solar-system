#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
// On commence à 7 car mesh.h utilise 0 à 6
layout (location = 7) in mat4 instanceMatrix; 

out vec2 TexCoords;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoords = aTexCoords;
    // On multiplie par la matrice spécifique à chaque astéroïde
    gl_Position = projection * view * instanceMatrix * vec4(aPos, 1.0);
}