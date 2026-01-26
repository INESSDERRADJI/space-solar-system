#include "planet.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// Lib includes
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// local includes
#include "camera.h"
#include "model.h"
#include "shader.h"

// For stbi_load used in loadCubemap (stb_image.h)
#include "stb_image.h"

static const GLint WIDTH = 1280, HEIGHT = 720;
static const double PI = 3.141592653589793238463;

static int SCREEN_WIDTH = WIDTH, SCREEN_HEIGHT = HEIGHT;
static GLFWwindow* window = nullptr;

// Function prototypes
static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
static void MouseCallback(GLFWwindow* window, double xPos, double yPos);
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
static unsigned int loadCubemap(const std::vector<std::string>& faces);
static void doMovement();

// Camera + input 
// Position initiale (vue globale)
static Camera camera(glm::vec3(-2200.0f, 260.0f, 2200.0f));

static bool keys[1024]{};
static GLfloat lastX = 400.0f, lastY = 300.0f;
static bool firstMouse = true;

//FR: zFar grand pour voir tout le système
static float zNear = 0.1f, zFar = 50000.0f;

// "" = free cam sinon planète focus
static std::string cameraType = "";

// Cursor lock (sans ImGui) SIMON
static bool menuActive = false;

// Timing
static GLfloat deltaTime = 0.0f;
static GLfloat lastFrame = 0.0f;

// Scene params
static glm::vec3 lightPos(0.0f, 0.0f, 0.0f);

static GLfloat scale = 1.0f;
static GLfloat AU = 149.597870f;
static GLfloat speed = 0.0000001f;
static GLfloat outerSpeed = 0.0001f;

// Post-processing
static bool bloomActive = true;
static bool lensFlareActive = true;
static int blurPasses = 7;

// Orbits
static bool showPlanetTrajectories = true;

// Anti-rebond clavier
static const double cooldownDuration = 0.25;
static double lastKeyPressTime = 0.0;


// On mémorise la position free cam pour la restaurer avec 0
static glm::vec3 freeCamPosBackup(0.0f);
static bool hasFreeCamBackup = false;

//Angles orbitaux autour de la planète (en degrés)
static float focusYaw = 45.0f;
static float focusPitch = 15.0f;

//Distance à la planète
static float focusDistance = 12.0f;

static const float trackpadLookSensitivity = 0.45f;
static const float trackpadMoveSpeed = 80.0f;
static const float trackpadOrbitSensitivity = 6.0f;


//Sensitivity for focus orbit
static const float focusMouseSensitivity = 0.12f;

static glm::vec3 orbitOffsetFromAngles(float yawDeg, float pitchDeg, float distance) {
    float yaw = glm::radians(yawDeg);
    float pitch = glm::radians(pitchDeg);

    glm::vec3 dir;
    dir.x = std::cos(pitch) * std::cos(yaw);
    dir.y = std::sin(pitch);
    dir.z = std::cos(pitch) * std::sin(yaw);

    return dir * distance;
}


// Helpers
static std::vector<glm::vec3> orbitCircle(float radius, int segments, const glm::vec3& center) {
    std::vector<glm::vec3> circlePoints;
    circlePoints.reserve(segments);
    for (int i = 0; i < segments; i++) {
        float theta = 2.0f * (float)PI * float(i) / float(segments);
        float x = center.x + radius * std::sin(theta);
        float z = center.z + radius * std::cos(theta);
        circlePoints.push_back(glm::vec3(x, center.y, z));
    }
    return circlePoints;
}

struct Sphere {
    glm::vec3 center;
    float radius;
};

static Sphere createSphere(float radius, glm::vec3 position) {
    Sphere sphere;
    sphere.center = position;
    sphere.radius = radius;
    return sphere;
}

static glm::vec3 orbitPos(bool move, GLuint i, float outerRadiusAU, float outerRotSpeedDeg) {
    if (!move) return glm::vec3(outerRadiusAU * AU * scale, 0.0f, 0.0f);

    float angle = outerRotSpeedDeg * (float)i;
    float radius = outerRadiusAU * AU * scale;

    float x = radius * std::sin((float)PI * 2.0f * angle / 360.0f);
    float z = radius * std::cos((float)PI * 2.0f * angle / 360.0f);
    return glm::vec3(x, 0.0f, z);
}

static glm::vec3 planetPosFor(const std::string& name, bool move, GLuint i) {
    if (name == "Mercury") return orbitPos(move, i, 0.39f, 49.9f * outerSpeed);
    if (name == "Venus")   return orbitPos(move, i, 0.72f, 35.0f * outerSpeed);
    if (name == "Earth")   return orbitPos(move, i, 1.00f, 29.8f * outerSpeed);
    if (name == "Mars")    return orbitPos(move, i, 1.52f, 24.1f * outerSpeed);
    if (name == "Jupiter") return orbitPos(move, i, 5.20f, 13.1f * outerSpeed);
    if (name == "Saturn")  return orbitPos(move, i, 9.54f, 9.7f * outerSpeed);
    if (name == "Uranus")  return orbitPos(move, i, 14.22f, 6.8f * outerSpeed);
    if (name == "Neptune") return orbitPos(move, i, 23.06f, 5.4f * outerSpeed);
    return lightPos;
}


// FR: Rayon visuel uniquement pour choisir une distance de focus correcte
static float visualRadiusFor(const std::string& name) {
    if (name == "Mercury") return 0.35f;
    if (name == "Venus")   return 1.0f;
    if (name == "Earth")   return 1.5f;
    if (name == "Mars")    return 0.9f;
    if (name == "Jupiter") return 15.0f;
    if (name == "Saturn")  return 12.0f;
    if (name == "Uranus")  return 10.0f;
    if (name == "Neptune") return 10.0f;
    return 1.0f;
}

static float focusDistanceFor(const std::string& name) {
    if (name == "Mercury") return 2.0f;

    if (name == "Venus")   return 6.0f;
    if (name == "Earth")   return 6.0f;
    if (name == "Mars")    return 3.0f;

    if (name == "Jupiter") return 80.0f;
    if (name == "Saturn")  return 85.0f;

    if (name == "Uranus")  return 40.0f;
    if (name == "Neptune") return 30.0f;

    return 150.0f;
}

static void setFocus(const std::string& name) {
    // Sauver la position free cam une fois
    if (cameraType.empty()) {
        freeCamPosBackup = camera.Position;
        hasFreeCamBackup = true;
    }

    cameraType = name;

    focusYaw = 45.0f;
    focusPitch = 15.0f;
    focusDistance = focusDistanceFor(name);
    std::cout << "[FOCUS] " << name << " focusDistance=" << focusDistance << std::endl;

    firstMouse = true;

    menuActive = false;
}

static void clearFocus() {
    cameraType.clear();
    if (hasFreeCamBackup) camera.Position = freeCamPosBackup;
    firstMouse = true;
}

//Draw helpers
static void draw_moon(const glm::vec3& pos, Model& moon, float moonOrbitRadius, Shader& shader) {
    GLfloat radius = AU * moonOrbitRadius;
    glm::mat4 moonModel = glm::mat4(1.0f);
    moonModel = glm::translate(moonModel, pos + glm::vec3(radius, 0.0f, radius));
    moonModel = glm::scale(moonModel, glm::vec3(0.6f, 0.6f, 0.6f));

    shader.use();
    shader.setMat4("model", moonModel);
    moon.Draw(shader);
}

static void draw_planet(
    bool move, unsigned int i,
    const glm::mat4& view, const glm::mat4& projection,
    float outerRadius, float innerRadius,
    float outerRotationSpeed, float innerRotationSpeed, float innerYawDeg,
    const std::string& name,
    Shader& shader, Shader& pathShader, Model& planet,
    Sphere* sphere = nullptr,
    Model* moon = nullptr, Shader* moonShader = nullptr,
    unsigned int nightTextureID = 0, unsigned int cloudTextureID = 0
) {
    (void)view; (void)projection; (void)pathShader;

    GLfloat angle = 0.0f, radius = 0.0f, x = 0.0f, z = 0.0f;
    glm::mat4 model(1.0f);
    glm::vec3 pos(0.0f);

    if (move) {
        angle = outerRotationSpeed * (GLfloat)i;
        radius = outerRadius * AU * scale;
        x = radius * std::sin((float)PI * 2.0f * angle / 360.0f);
        z = radius * std::cos((float)PI * 2.0f * angle / 360.0f);
        model = glm::translate(model, glm::vec3(x, 0.0f, z));
        pos = glm::vec3(x, 0.0f, z);
        if (sphere) sphere->center = pos;
    }
    else {
        model = glm::translate(model, glm::vec3(outerRadius * scale, 0.0f, 0.0f));
    }

    // Spin
    angle = innerRotationSpeed * (GLfloat)i * 1.35f;
    model = glm::rotate(model, glm::radians(innerYawDeg) + angle, glm::vec3(0.0f, 0.1f, 0.0f));
    model = glm::scale(model, glm::vec3(innerRadius * scale));

    shader.use();
    shader.setMat4("model", model);

    // Earth special (day/night/clouds)
    if (name == "Earth") {
        planet.Draw2(shader, "night", nightTextureID, "cloud", cloudTextureID, glfwGetTime());
        if (moon && moonShader) draw_moon(pos, *moon, 0.035f, *moonShader);
        return;
    }

    planet.Draw(shader);
}

// Lens flare ray test
static bool isIntersecting(glm::vec3 rayOrigin, glm::vec3 rayDirection,
    const Sphere& sphere, float correction = 1.0f) {
    float cameraToPlanetLength = glm::length(sphere.center - camera.Position);
    float cutoff = cameraToPlanetLength / AU * correction;

    glm::vec3 oc = rayOrigin - sphere.center;
    float a = glm::dot(rayDirection, rayDirection);
    float b = 2.0f * glm::dot(oc, rayDirection);

    float r = glm::clamp(sphere.radius - cutoff, 0.0f, sphere.radius);
    float c = glm::dot(oc, oc) - (r * r);

    float discriminant = b * b - 4 * a * c;
    if (discriminant > 0) {
        float t1 = (-b - std::sqrt(discriminant)) / (2.0f * a);
        float t2 = (-b + std::sqrt(discriminant)) / (2.0f * a);
        if ((t1 >= 0.0f) || (t2 >= 0.0f)) return true;
    }
    return false;
}


// Main loop entry

int system() {
    bool move = true;

    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Solar System", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwGetFramebufferSize(window, &SCREEN_WIDTH, &SCREEN_HEIGHT);

    glfwSetKeyCallback(window, KeyCallback);
    glfwSetCursorPosCallback(window, MouseCallback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glewExperimental = GL_TRUE;
    if (GLEW_OK != glewInit()) {
        std::cerr << "Failed to initialize GLEW\n";
        return EXIT_FAILURE;
    }

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glfwSwapInterval(0);

    Shader shader("resources/shaders/modelLoading.vs", "resources/shaders/modelLoading.frag");
    Shader earthShader("resources/shaders/earth.vs", "resources/shaders/earth.frag");
    Shader pathShader("resources/shaders/path.vs", "resources/shaders/path.frag");
    Shader skyboxShader("resources/shaders/skybox.vs", "resources/shaders/skybox.frag");
    Shader lampShader("resources/shaders/lamp.vs", "resources/shaders/lamp.frag");
    Shader screenShader("resources/shaders/framebuffer.vs", "resources/shaders/framebuffer.frag");
    Shader blurShader("resources/shaders/blur.vs", "resources/shaders/blur.frag");

    float sunRadius = 50.0f;
    float earthRadius = 1.5f;
    float mercuryRadius = 0.35f;
    float venusRadius = 1.0f;
    float marsRadius = 0.9f;
    float jupiterRadius = 15.0f;
    float saturnRadius = 12.0f;
    float uranusRadius = 10.0f;
    float neptuneRadius = 10.0f;

    // Load models
    Model earthModel("resources/models/earth/earth.obj");
    Model sunModel("resources/models/sun/sun.obj");
    Model mercuryModel("resources/models/mercury/mercury.obj");
    Model venusModel("resources/models/venus/venus.obj");
    Model marsModel("resources/models/mars/mars.obj");
    Model jupiterModel("resources/models/jupiter/jupiter.obj");
    Model saturnModel("resources/models/saturn/saturn.obj");
    Model uranusModel("resources/models/uranus/uranus.obj");
    Model neptuneModel("resources/models/neptune/neptune.obj");
    Model moonModel("resources/models/moon/moon.obj");

    // Spheres for ray cast (lens flare)
    Sphere sunSphere = createSphere(sunRadius, lightPos);
    Sphere earthSphere = createSphere(earthRadius, glm::vec3(0.0f, 0.0f, 1.0f * AU));
    Sphere mercurySphere = createSphere(mercuryRadius, glm::vec3(0.0f, 0.0f, 0.39f * AU));
    Sphere venusSphere = createSphere(venusRadius, glm::vec3(0.0f, 0.0f, 0.72f * AU));
    Sphere marsSphere = createSphere(marsRadius, glm::vec3(0.0f, 0.0f, 1.52f * AU));
    Sphere jupiterSphere = createSphere(jupiterRadius, glm::vec3(0.0f, 0.0f, 5.2f * AU));
    Sphere saturnSphere = createSphere(saturnRadius, glm::vec3(0.0f, 0.0f, 9.54f * AU));
    Sphere uranusSphere = createSphere(uranusRadius, glm::vec3(0.0f, 0.0f, 14.22f * AU));
    Sphere neptuneSphere = createSphere(neptuneRadius, glm::vec3(0.0f, 0.0f, 23.06f * AU));

    unsigned int earthNightTextureID = TextureFromFile("resources/models/earth/earthnight.jpg", ".");
    unsigned int earthCloudTextureID = TextureFromFile("resources/models/earth/earthclouds.jpg", ".");
    unsigned int noiseTextureID = TextureFromFile("resources/others/noise.png", ".");

    
    shader.use();
    shader.setVec3("light.position", lightPos);
    shader.setVec3("light.ambient", 0.18f, 0.18f, 0.18f);
    shader.setVec3("light.diffuse", 1.10f, 1.10f, 1.10f);
    shader.setVec3("light.specular", 0.25f, 0.25f, 0.25f);
    shader.setFloat("light.constant", 1.5f);
    shader.setFloat("light.linear", 0.0000002f);
    shader.setFloat("light.quadratic", 0.0000006f);

    earthShader.use();
    earthShader.setVec3("light.position", lightPos);
    earthShader.setVec3("light.ambient", 0.18f, 0.18f, 0.18f);
    earthShader.setVec3("light.diffuse", 1.10f, 1.10f, 1.10f);
    earthShader.setVec3("light.specular", 0.25f, 0.25f, 0.25f);
    earthShader.setFloat("light.constant", 1.0f);
    earthShader.setFloat("light.linear", 0.0000002f);
    earthShader.setFloat("light.quadratic", 0.0000006f);

    // EN: Sun glow intensity 
    lampShader.use();
    lampShader.setFloat("sunIntensity", 120.0f); // si plus 200.5f

    // SKYBOX 
    float skyboxVertices[] = {
        -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
        1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,

        -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
        -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

        1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,

        -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

        -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,

        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f,
        1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f
    };

    unsigned int skyboxVAO = 0, skyboxVBO = 0;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    std::vector<std::string> faces{
        "resources/skybox/bkg1_right.png", "resources/skybox/bkg1_left.png",
        "resources/skybox/bkg1_top.png",   "resources/skybox/bkg1_bot.png",
        "resources/skybox/bkg1_front.png", "resources/skybox/bkg1_back.png",
    };
    unsigned int cubemapTexture = loadCubemap(faces);

    //  Post processing 
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f, 0.0f, 1.0f,  -1.0f, -1.0f, 0.0f, 0.0f,   1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,   1.0f, -1.0f, 1.0f, 0.0f,   1.0f,  1.0f, 1.0f, 1.0f
    };

    unsigned int quadVAO = 0, quadVBO = 0;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    screenShader.use();
    screenShader.setInt("screenTexture", 0);
    screenShader.setInt("bloomBlur", 1);
    screenShader.setVec3("lightPos", lightPos);
    screenShader.setInt("screen_width", SCREEN_WIDTH);
    screenShader.setInt("screen_height", SCREEN_HEIGHT);
    screenShader.setInt("noise_texture", 2);

    blurShader.use();
    blurShader.setInt("image", 0);

    // Framebuffer (scene + bright)
    unsigned int framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    unsigned int textureColorbuffer = 0;
    glGenTextures(1, &textureColorbuffer);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

    unsigned int bloomTexture = 0;
    glGenTextures(1, &bloomTexture);
    glBindTexture(GL_TEXTURE_2D, bloomTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, bloomTexture, 0);

    unsigned int rbo = 0;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCREEN_WIDTH, SCREEN_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Ping-pong for blur
    unsigned int pingpongFBO[2]{ 0, 0 };
    unsigned int pingpongColorbuffers[2]{ 0, 0 };
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int k = 0; k < 2; k++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[k]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[k]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[k], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "Pingpong framebuffer not complete!\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    int speedModifier = 1;

    // FPS to title
    int frameCount = 0;
    double fps = 0.0;
    float lastTime = (float)glfwGetTime();
    float fpsDeltaTime = 0.0f;

    GLuint i = 0;

    while (!glfwWindowShouldClose(window)) {
        GLfloat currentFrame = (GLfloat)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        fpsDeltaTime = currentFrame - lastTime;
        lastFrame = currentFrame;
        frameCount++;

        if (fpsDeltaTime >= 1.0f) {
            fps = frameCount / fpsDeltaTime;
            frameCount = 0;
            lastTime = currentFrame;

            std::string title = "Solar System - FPS: " + std::to_string((int)fps);
            glfwSetWindowTitle(window, title.c_str());
        }

        // Cursor lock/unlock (free + focus)
        glfwSetInputMode(window, GLFW_CURSOR, menuActive ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);

        // Inputs
        doMovement();

        i += (GLuint)std::max(0, speedModifier);
        if (i == UINT_MAX) i = 0;

        // Render to HDR framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        glClearColor(0.00f, 0.00f, 0.00f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view(1.0f);

        if (cameraType.empty()) {
            view = camera.GetViewMatrix();
        }
        else {
          
            // Focus cam orbite autour de la planète sélectionnée (souris = yaw/pitch)
            glm::vec3 target = planetPosFor(cameraType, move, i);
            glm::vec3 offset = orbitOffsetFromAngles(focusYaw, focusPitch, focusDistance);
            camera.Position = target + offset;
            view = glm::lookAt(camera.Position, target, glm::vec3(0.0f, 1.0f, 0.0f));
        }

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
            (float)WIDTH / (float)HEIGHT, zNear, zFar);

        shader.use();
        shader.setVec3("viewPos", camera.Position);
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        pathShader.use();
        pathShader.setMat4("projection", projection);
        pathShader.setMat4("view", view);

        // Planets
        draw_planet(move, i, view, projection, 0.39f, 1.0f, 49.9f * outerSpeed, 10.83f * speed, 0.0f,
            "Mercury", shader, pathShader, mercuryModel, &mercurySphere);

        draw_planet(move, i, view, projection, 0.72f, 1.0f, 35.0f * outerSpeed, 6.52f * speed, 0.0f,
            "Venus", shader, pathShader, venusModel, &venusSphere);

        earthShader.use();
        earthShader.setVec3("viewPos", camera.Position);
        earthShader.setMat4("projection", projection);
        earthShader.setMat4("view", view);

        draw_planet(move, i, view, projection, 1.0f, 1.4f, 29.8f * outerSpeed, 1574.0f * speed, 0.0f,
            "Earth", earthShader, pathShader, earthModel, &earthSphere,
            &moonModel, &shader, earthNightTextureID, earthCloudTextureID);

        draw_planet(move, i, view, projection, 1.52f, 1.0f, 24.1f * outerSpeed, 866.0f * speed, 0.0f,
            "Mars", shader, pathShader, marsModel, &marsSphere);

        draw_planet(move, i, view, projection, 5.20f, 1.0f, 13.1f * outerSpeed, 45583.0f * speed, 0.0f,
            "Jupiter", shader, pathShader, jupiterModel, &jupiterSphere);

        draw_planet(move, i, view, projection, 9.54f, 1.0f, 9.7f * outerSpeed, 36840.0f * speed, 90.0f,
            "Saturn", shader, pathShader, saturnModel, &saturnSphere);

        draw_planet(move, i, view, projection, 14.22f, 1.0f, 6.8f * outerSpeed, 14797.0f * speed, 160.0f,
            "Uranus", shader, pathShader, uranusModel, &uranusSphere);

        draw_planet(move, i, view, projection, 23.06f, 1.0f, 5.4f * outerSpeed, 9719.0f * speed, 130.0f,
            "Neptune", shader, pathShader, neptuneModel, &neptuneSphere);

        // SUN
        lampShader.use();
        lampShader.setMat4("view", view);
        lampShader.setMat4("projection", projection);

        glm::mat4 sunM(1.0f);
        sunM = glm::translate(sunM, lightPos);
        sunM = glm::scale(sunM, glm::vec3(scale));
        lampShader.setMat4("model", sunM);
        sunModel.Draw(lampShader);

        // Orbit lines
        if (showPlanetTrajectories) {
            GLuint orbitVBO = 0, orbitVAO = 0;
            glGenVertexArrays(1, &orbitVAO);
            glGenBuffers(1, &orbitVBO);

            glBindVertexArray(orbitVAO);
            glBindBuffer(GL_ARRAY_BUFFER, orbitVBO);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

            pathShader.use();
            pathShader.setVec3("pathColor", glm::vec3(0.15f, 0.15f, 0.15f));
            pathShader.setMat4("model", glm::mat4(1.0f));

            auto drawOrbit = [&](float rAU) {
                float r = rAU * AU * scale;
                std::vector<glm::vec3> pts = orbitCircle(r, 160, lightPos);
                glBufferData(GL_ARRAY_BUFFER, pts.size() * sizeof(glm::vec3), pts.data(), GL_STATIC_DRAW);
                glDrawArrays(GL_LINE_LOOP, 0, (GLsizei)pts.size());
                };

            drawOrbit(0.39f);
            drawOrbit(0.72f);
            drawOrbit(1.0f);
            drawOrbit(1.52f);
            drawOrbit(5.2f);
            drawOrbit(9.54f);
            drawOrbit(14.22f);
            drawOrbit(23.06f);

            glBindVertexArray(0);
            glDeleteBuffers(1, &orbitVBO);
            glDeleteVertexArrays(1, &orbitVAO);
        }

        // SKYBOX
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        glm::mat4 viewNoTranslate = glm::mat4(glm::mat3(view));
        skyboxShader.setMat4("view", viewNoTranslate);
        skyboxShader.setMat4("projection", projection);

        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        // Bloom blur
        bool horizontal = true, first_iteration = true;
        if (bloomActive) {
            blurShader.use();
            for (int pass = 0; pass < blurPasses; pass++) {
                glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal ? 1 : 0]);
                blurShader.setInt("horizontal", horizontal ? 1 : 0);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, first_iteration ? bloomTexture : pingpongColorbuffers[horizontal ? 0 : 1]);

                glBindVertexArray(quadVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);

                horizontal = !horizontal;
                if (first_iteration) first_iteration = false;
            }
        }

        // Final pass
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Compute sun screen position for lens flare shader
        glm::vec4 sunClip = (projection * view) * glm::vec4(lightPos, 1.0f);
        sunClip /= sunClip.w;

        glm::vec3 sunScreenPos(
            (sunClip.x + 1.0f) * 0.5f * SCREEN_WIDTH,
            (sunClip.y + 1.0f) * 0.5f * SCREEN_HEIGHT,
            (sunClip.z + 1.0f) * 0.5f
        );

        screenShader.use();
        screenShader.setMat4("view", view);
        screenShader.setMat4("projection", projection);
        screenShader.setVec3("screenLightPos", sunScreenPos);
        screenShader.setBool("bloomActive", bloomActive);

        // Lens flare visibility test
        if (lensFlareActive) {
            glm::vec3 rayDirection = glm::normalize(sunSphere.center - camera.Position);
            bool sunVisible =
                !isIntersecting(camera.Position, rayDirection, mercurySphere, 10.0f) &&
                !isIntersecting(camera.Position, rayDirection, venusSphere, 2.0f) &&
                !isIntersecting(camera.Position, rayDirection, earthSphere, 2.0f) &&
                !isIntersecting(camera.Position, rayDirection, marsSphere) &&
                !isIntersecting(camera.Position, rayDirection, jupiterSphere, 1.5f) &&
                !isIntersecting(camera.Position, rayDirection, saturnSphere) &&
                !isIntersecting(camera.Position, rayDirection, uranusSphere) &&
                !isIntersecting(camera.Position, rayDirection, neptuneSphere);
            screenShader.setBool("sunVisibleAndEnabled", sunVisible);
        }
        else {
            screenShader.setBool("sunVisibleAndEnabled", false);
        }

        // Noise texture
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, noiseTextureID);

        glBindVertexArray(quadVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[horizontal ? 0 : 1]);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}


// Movement + toggles
static void doMovement() {

    // Déplacement WASD uniquement en free cam
    if (cameraType.empty()) {
        if (keys[GLFW_KEY_W] || keys[GLFW_KEY_UP])    camera.ProcessKeyboard(FORWARD, deltaTime);
        if (keys[GLFW_KEY_S] || keys[GLFW_KEY_DOWN])  camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (keys[GLFW_KEY_A] || keys[GLFW_KEY_LEFT])  camera.ProcessKeyboard(LEFT, deltaTime);
        if (keys[GLFW_KEY_D] || keys[GLFW_KEY_RIGHT]) camera.ProcessKeyboard(RIGHT, deltaTime);
    }
    else {
        if (keys[GLFW_KEY_W] || keys[GLFW_KEY_S] || keys[GLFW_KEY_A] || keys[GLFW_KEY_D]) {
           
            // std::cout << "Press 0 to return to free camera\n";
        }
    }

    double currentTime = glfwGetTime();
    auto canToggle = [&]() {
        if (currentTime - lastKeyPressTime > cooldownDuration) {
            lastKeyPressTime = currentTime;
            return true;
        }
        return false;
        };

    // P lock/unlock cursor
    if (keys[GLFW_KEY_P] && canToggle()) { menuActive = !menuActive; firstMouse = true; }

    // T orbit lines
    if (keys[GLFW_KEY_T] && canToggle()) { showPlanetTrajectories = !showPlanetTrajectories; }

    // B bloom
    if (keys[GLFW_KEY_B] && canToggle()) { bloomActive = !bloomActive; }

    // F lens flare
    if (keys[GLFW_KEY_F] && canToggle()) { lensFlareActive = !lensFlareActive; }

    // [ / ] blur passes
    if (keys[GLFW_KEY_LEFT_BRACKET] && canToggle())  blurPasses = std::max(1, blurPasses - 1);
    if (keys[GLFW_KEY_RIGHT_BRACKET] && canToggle()) blurPasses = std::min(10, blurPasses + 1);
}


// Callbacks

static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    (void)scancode; (void)mode;

    if (GLFW_KEY_ESCAPE == key && GLFW_PRESS == action) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) keys[key] = true;
        else if (action == GLFW_RELEASE) keys[key] = false;
    }

   
    // Focus sur appui
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_1) { setFocus("Mercury"); return; }
        if (key == GLFW_KEY_2) { setFocus("Venus");   return; }
        if (key == GLFW_KEY_3) { setFocus("Earth");   return; }
        if (key == GLFW_KEY_4) { setFocus("Mars");    return; }
        if (key == GLFW_KEY_5) { setFocus("Jupiter"); return; }
        if (key == GLFW_KEY_6) { setFocus("Saturn");  return; }
        if (key == GLFW_KEY_7) { setFocus("Uranus");  return; }
        if (key == GLFW_KEY_8) { setFocus("Neptune"); return; }
        if (key == GLFW_KEY_0) { clearFocus();        return; }

        if (key == GLFW_KEY_SPACE && !cameraType.empty()) { clearFocus(); return; }
    }

}

static void MouseCallback(GLFWwindow* window, double xPos, double yPos) {
    (void)window;

    if (firstMouse) {
        lastX = (GLfloat)xPos;
        lastY = (GLfloat)yPos;
        firstMouse = false;
        return;
    }

    GLfloat xOffset = (GLfloat)xPos - lastX;
    GLfloat yOffset = lastY - (GLfloat)yPos;

    lastX = (GLfloat)xPos;
    lastY = (GLfloat)yPos;

    if (menuActive) return;

    if (cameraType.empty()) {
        camera.ProcessMouseMovement(xOffset, yOffset);
        return;
    }

    focusYaw += xOffset * focusMouseSensitivity;
    focusPitch += yOffset * focusMouseSensitivity;
    focusPitch = glm::clamp(focusPitch, -89.0f, 89.0f);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (menuActive) return;

    const bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

    if (cameraType.empty())
    {
        if (shift)
        {
            float forward = -(float)yoffset * trackpadMoveSpeed;
            float strafe = (float)xoffset * trackpadMoveSpeed;

            camera.Position += camera.Front * forward;
            camera.Position += camera.Right * strafe;
        }
        else
        {
            camera.ProcessMouseScroll((float)yoffset);
        }
        return;
    }

    if (shift)
    {
        focusYaw += (float)xoffset * trackpadOrbitSensitivity;
        focusPitch += (float)-yoffset * trackpadOrbitSensitivity;
        focusPitch = glm::clamp(focusPitch, -89.0f, 89.0f);
    }
    else
    {
        focusDistance -= (float)yoffset * 10.0f;
        focusDistance = glm::clamp(focusDistance, 5.0f, 5000.0f);
    }
}


static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
}


static unsigned int loadCubemap(const std::vector<std::string>& faces) {
    unsigned int textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width = 0, height = 0, nrChannels = 0;

    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);

        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            GLenum internal = (nrChannels == 4) ? GL_SRGB_ALPHA : GL_SRGB;

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal,
                width, height, 0, format, GL_UNSIGNED_BYTE, data);

            stbi_image_free(data);
        }
        else {
            std::cerr << "Cubemap texture failed to load at path: " << faces[i] << "\n";
            stbi_image_free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}
