#include <glad/glad.h>
#include <cstddef>
#include <GLFW/glfw3.h>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "shader.h"
#include "camera.h"
#include "stb_image.h"
#include "model.h"

#include <filesystem>
#include <random>
namespace fs = std::filesystem;

// Global pointer to stencil pass shader so helper functions can access it
Shader* gStencilShader = nullptr;

// Debug toggle to visualise SSAO texture
bool gShowSSAO = false;

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
unsigned int loadTexture(const char *path, bool gammaCorrection = true);
void renderScene(const Shader &shader);
void renderSphere();
void renderCube();
void renderQuad();
void stencilPassTest(const glm::vec3 &lightPos, float radius, const glm::mat4 &view, const glm::mat4 &projection);
void showFPS(GLFWwindow* window);
// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Time
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Mouse initial position
float lastX = SCR_WIDTH / 2.0;
float lastY = SCR_HEIGHT / 2.0;
bool firstMouse = true;
float fov = 45.0f;
float exposure = 5.0f;

Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));

// Vertices coordinates
GLfloat vertices[] =
    {
        // positions            // normals         // texcoords
        25.0f, -0.5f, 25.0f, 0.0f, 1.0f, 0.0f, 25.0f, 0.0f,
        -25.0f, -0.5f, 25.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        -25.0f, -0.5f, -25.0f, 0.0f, 1.0f, 0.0f, 0.0f, 25.0f,

        25.0f, -0.5f, 25.0f, 0.0f, 1.0f, 0.0f, 25.0f, 0.0f,
        -25.0f, -0.5f, -25.0f, 0.0f, 1.0f, 0.0f, 0.0f, 25.0f,
        25.0f, -0.5f, -25.0f, 0.0f, 1.0f, 0.0f, 25.0f, 25.0f};

// lighting info
// -------------
// glm::vec3 lightPositions[] = {
//     glm::vec3(-3.0f, 0.0f, 0.0f),
//     glm::vec3(-1.0f, 0.0f, 0.0f),
//     glm::vec3(1.0f, 0.0f, 0.0f),
//     glm::vec3(3.0f, 0.0f, 0.0f)};
// glm::vec3 lightColors[] = {
//     glm::vec3(0.25),
//     glm::vec3(0.50),
//     glm::vec3(0.75),
//     glm::vec3(1.00)};

// meshes
unsigned int planeVAO;
unsigned int cubeVAO, cubeVBO = 0;

unsigned int quadVAO = 0;
unsigned int quadVBO;

unsigned int sphereVAO = 0, sphereVBO = 0;

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}


int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // create window
    GLFWwindow *window = glfwCreateWindow(800, 600, "Lighting Example", nullptr, nullptr);
    if (window == NULL)
    {
        std::cout << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    std::string parentDir = (fs::current_path().fs::path::parent_path()).string();
    Shader modelShader("resources/shaders/model.vs", "resources/shaders/model.fs");
    Shader lightingPassShader("resources/shaders/lightPass.vs", "resources/shaders/commonLightPass.fs");
    Shader lightSourceShader("resources/shaders/lightSource.vs", "resources/shaders/lightSource.fs");
    gStencilShader = new Shader("resources/shaders/stencilPass.vs", "resources/shaders/stencilPass.fs");
    Shader shaderSSAO("resources/shaders/ssao.vs", "resources/shaders/ssao.fs");
    Shader shaderSSAOBlur("resources/shaders/ssao.vs", "resources/shaders/ssaoBlur.fs");
    Shader debugSphereShader("resources/shaders/debugSphere.vs", "resources/shaders/debugSphere.fs");
    Shader ambientLightPassShader("resources/shaders/ambientLightPass.vs", "resources/shaders/ambientLightPass.fs");
    // Shader to display raw SSAO texture for debugging
    Shader ssaoDebugShader("resources/shaders/ambientLightPass.vs", "resources/shaders/showSSAO.fs");

    GLuint planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);

    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // Position Attrib
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (GLvoid *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (GLvoid *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (GLvoid *)(6 * sizeof(float)));
    glBindVertexArray(0);

    // Load Models
    // --------------------------------------------------
    auto *player = new Model(parentDir + "/resources/objects/backpack/backpack.obj");
    std::vector<glm::vec3> objectPositions;
    objectPositions.emplace_back(-3.0, -0.5, -3.0);
    objectPositions.emplace_back(0.0, -0.5, -3.0);
    objectPositions.emplace_back(3.0, -0.5, -3.0);
    objectPositions.emplace_back(-3.0, -0.5, 0.0);
    objectPositions.emplace_back(0.0, -0.5, 0.0);
    objectPositions.emplace_back(3.0, -0.5, 0.0);
    objectPositions.emplace_back(-3.0, -0.5, 3.0);
    objectPositions.emplace_back(0.0, -0.5, 3.0);
    objectPositions.push_back(glm::vec3(3.0, -0.5, 3.0));

    // Generating random sample kernel with 64 sample values
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;
    std::vector<glm::vec3> ssaoKernel;
    for (unsigned int i=0; i<64; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator)
        );
        sample = glm::normalize(sample);
        // sample *= randomFloats(generator);
        float scale = (float)i / 64.0f;
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sample);
    }

    // Generating noise Texture
    // used to rotate sample kernel
    std::vector<glm::vec3> ssaoNoise;
    for (unsigned int i=0; i<16; i++) {
        glm::vec3 noise(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            0.0f
        );
        ssaoNoise.push_back(noise);
    }

    // Geometry Buffers framebuffer
    // ---------------------------------------------------
    unsigned int gBuffer;
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    unsigned int gPosition, gNormal, gColorSpec;

    // position color buffer;
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

    // normal color buffer;
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

    // color + specular color buffer;
    glGenTextures(1, &gColorSpec);
    glBindTexture(GL_TEXTURE_2D, gColorSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gColorSpec, 0);

    constexpr unsigned int attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, attachments);

    // Add rbo as depth buffer to check completeness
    unsigned int grboDepth;
    glGenRenderbuffers(1, &grboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, grboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, grboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Kernel Noise (Rotation) Buffer (4×4 tileable noise)
    // -----------------------------------------------------
    const unsigned int noiseDim = 4;
    unsigned int noiseTexture;
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    // Store as RGB; alpha channel not needed
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, noiseDim, noiseDim, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // framebuffer to store render output of ssao shader
    // -----------------------------------------------------
    unsigned int ssaoFBO;
    glGenFramebuffers(1, &ssaoFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);

    // ambient occlusion color buffer
    unsigned int ssaoColorBuffer;
    glGenTextures(1, &ssaoColorBuffer);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SCR_WIDTH, SCR_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "SSAO Framebuffer not complete!" << std::endl;

    // blur fbo
    unsigned int ssaoBlurFBO, ssaoColorBufferBlur;
    glGenFramebuffers(1, &ssaoBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glGenTextures(1, &ssaoColorBufferBlur);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SCR_WIDTH, SCR_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBufferBlur, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "SSAO Blur Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);



    // lighting info
    // -------------
    constexpr unsigned int NR_LIGHTS = 1;
    std::vector<glm::vec3> lightPositions;
    std::vector<glm::vec3> lightColors;
    srand(13);
    for (unsigned int i = 0; i < NR_LIGHTS; i++) {
        // calculate slightly random offsets
        auto lightPos = glm::vec3(2.0, 4.0, -2.0);
        lightPositions.push_back(lightPos);
        // also calculate random color
        float rColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
        float gColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
        float bColor = static_cast<float>(((rand() % 100) / 200.0f) + 0.5); // between 0.5 and 1.0
        lightColors.push_back(glm::vec3(0.2, 0.2, 0.7));
    }

    modelShader.use();
    modelShader.setInt("texture_diffuse1", 0);
    modelShader.setInt("texture_specular1", 1);

    lightingPassShader.use();
    lightingPassShader.setInt("gPosition", 0);
    lightingPassShader.setInt("gNormal", 1);
    lightingPassShader.setInt("gColorSpec", 2);
    lightingPassShader.setVec2("screenSize", glm::vec2(SCR_WIDTH, SCR_HEIGHT));

    shaderSSAO.use();
    shaderSSAO.setInt("gPosition", 0);
    shaderSSAO.setInt("gNormal", 1);
    shaderSSAO.setInt("texNoise", 2);

    shaderSSAOBlur.use();
    shaderSSAOBlur.setInt("ssaoInput", 0);

    ambientLightPassShader.use();
    ambientLightPassShader.setInt("gPosition", 0);
    ambientLightPassShader.setInt("gNormal", 1);
    ambientLightPassShader.setInt("gColorSpec", 2);
    ambientLightPassShader.setInt("ssao", 3);

    ssaoDebugShader.use();
    ssaoDebugShader.setInt("ssaoTex", 0);

    while (!glfwWindowShouldClose(window))
    {

        // Update window title with FPS information
        showFPS(window);

        // calculate delta Time
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Implementing geometry pass
        // --------------------------------------------------
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer); 
        glDepthMask(GL_TRUE); // Only geometry pass writes to depth buffer
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        modelShader.use();
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 50.0f);
        modelShader.setMat4("view", view);
        modelShader.setMat4("projection", projection);

        renderScene(modelShader);
        player->Draw(modelShader);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        // Implementing lighting pass
        // --------------------------------------------------
        // glEnable(GL_BLEND);
        // glBlendEquation(GL_FUNC_ADD);
        // glBlendFunc(GL_ONE, GL_ONE);

        // Switch to default framebuffer to accumulate lighting results on screen
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        shaderSSAO.use();
        // Bind G-buffer textures for rendering ssao and lighting passes
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gPosition);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, noiseTexture);
        // render SSAO texture
        // -------------------------------------------------
        
        for (unsigned int i = 0; i < ssaoKernel.size(); ++i) {
            shaderSSAO.setVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
        }
        shaderSSAO.setMat4("projection", projection);
        shaderSSAO.setVec2("noiseScale", glm::vec2(SCR_WIDTH / 4.0f, SCR_HEIGHT / 4.0f));
        renderQuad();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);


        // Ambient Occlusion blur to remove the repeated pattern
        // -----------------------------------------------------
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        shaderSSAOBlur.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
        renderQuad();

        // // If debug mode is active, output SSAO texture directly and skip further passes
        // if (gShowSSAO) {
        //     glBindFramebuffer(GL_FRAMEBUFFER, 0);
        //     glClear(GL_COLOR_BUFFER_BIT);
        //     ssaoDebugShader.use();
        //     glActiveTexture(GL_TEXTURE0);
        //     glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
        //     renderQuad();

        //     glfwSwapBuffers(window);
        //     glfwPollEvents();
        //     continue; // skip rest of loop so only SSAO is shown
        // }

        glDepthMask(GL_FALSE); // Disable depth writing for the lighting pass
        // --------------------------------------------------
        // Point lights pass
        // --------------------------------------------------
        // We need stencil to be enabled in the stencil pass to get the stencil buffer
        // updated and we also need it in the light pass because we render the light
        // only if the stencil passes.
        glEnable(GL_STENCIL_TEST);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        for (unsigned int i = 0; i < lightPositions.size(); i++)
        {
            // Calculate light radius
            constexpr float linear = 0.09f;
            constexpr float quadratic = 0.032f;
            float constant = 1.0f; 
            const float maxBrightness = std::fmaxf( std::fmaxf(lightColors[i].r, lightColors[i].g), lightColors[i].b);
            float radius = (-linear + std::sqrt(linear * linear - 4 * quadratic * (constant - (256.0f / 5.0f) * maxBrightness))) / (2.0f * quadratic);

            // Stencil pass for this light
            stencilPassTest(lightPositions[i], radius, view, projection);
            
            // Point light pass for this light
            lightingPassShader.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, gPosition);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, gNormal);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, gColorSpec);
            lightingPassShader.setBool("useQuadRendering", false);
            lightingPassShader.setVec3("light.Position", glm::vec3(view * glm::vec4(lightPositions[i], 1.0f)));
            lightingPassShader.setVec3("light.Color", lightColors[i]);
            lightingPassShader.setFloat("light.Linear", linear);
            lightingPassShader.setFloat("light.Quadratic", quadratic);
            // as FragPos is in view-space and calculation is done in view space
            lightingPassShader.setVec3("viewPos", glm::vec3(0.0f));

            // Set up stencil test to only render where light volume affects geometry
            // must disable the draw buffers
            glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
            glDisable(GL_DEPTH_TEST);
            
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ONE, GL_ONE);

            // Render back faces of light sphere to avoid issues when camera is inside
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            
            // Render the light volume sphere
            glm::mat4 model = glm::translate(glm::mat4(1.0f), lightPositions[i]);
            model = glm::scale(model, glm::vec3(radius));
            lightingPassShader.setMat4("model", model);
            lightingPassShader.setMat4("view", view);
            lightingPassShader.setMat4("projection", projection);
            
            renderSphere();
            
            // Reset culling
            glCullFace(GL_BACK);
            glDisable(GL_BLEND);
        }

        // --------------------------------------------------
        // After all point lights rendered: run ambient/directional pass
        // --------------------------------------------------
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ONE);
        glDrawBuffer(GL_BACK);

        ambientLightPassShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gPosition);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gColorSpec);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
        ambientLightPassShader.setVec3("lightColor", glm::vec3(1.0f));
        renderQuad();

        // Restore depth test and disable blend for any subsequent passes
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        // copy depth info from gpass into the default framebuffer depth buffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);  // write to default buffer
        glBlitFramebuffer(
            0, 0, SCR_WIDTH, SCR_HEIGHT, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST
        );
        glBindFramebuffer(GL_FRAMEBUFFER, 0);



        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glfwDestroyWindow(window);
    // Terminate GLFW, clearing any resources allocated by GLFW.
    glfwTerminate();
    return 0;
}

void stencilPassTest(const glm::vec3 &lightPos, float radius, const glm::mat4 &view, const glm::mat4 &projection) {

    // Disable colour writes and depth writes; we only want to update the stencil buffer
    // Disable colour writes so the bounding sphere is not drawn to the colour buffer
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    // Disable depth writes so we keep the geometry pass depth intact
    glDepthMask(GL_FALSE);

    // Disable colour writes, don't update depth
    // must disable the draw buffers

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glClear(GL_STENCIL_BUFFER_BIT);

    // Increment stencil on back-faces, decrement on front-faces
    glStencilFunc(GL_ALWAYS, 0, 0);
    glStencilOpSeparate(GL_BACK,  GL_KEEP, GL_INCR_WRAP, GL_KEEP);
    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);

    // Render bounding sphere with minimal shader
    gStencilShader->use();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), lightPos);
    model = glm::scale(model, glm::vec3(radius));
    glm::mat4 mvp = projection * view * model;
    gStencilShader->setMat4("MVP", mvp);

    // Actually draw the sphere!
    renderSphere();

    // Restore default write masks after stencil pass
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

}

void renderCube()
{
    if (cubeVAO == 0)
    {
        float cubeVertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right
            1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,  // bottom-right
            1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right
            -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,  // top-left
            // front face
            -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
            1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  // bottom-right
            1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right
            1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right
            -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,  // top-left
            -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right
            -1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  // top-left
            -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
            -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right
                                                                // right face
            1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,     // top-left
            1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   // bottom-right
            1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,    // top-right
            1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   // bottom-right
            1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,     // top-left
            1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,    // bottom-left
            // bottom face
            -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
            1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,  // top-left
            1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left
            1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left
            -1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
            -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left
            1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right
            1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,  // top-right
            1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right
            -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left
            -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f   // bottom-left
        };

        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);

        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
        glBindVertexArray(cubeVAO);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void renderQuad()
{
    if (quadVAO == 0)
    {
        float QuadVertices[] = {
            // positions    // texCoords
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, // bottom left
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f, // top left
            1.0f, -1.0f, 0.0f, 1.0f, 0.0f, // bottom right
            1.0f,  1.0f, 0.0f, 1.0f, 1.0f  // top right
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(QuadVertices), &QuadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void renderSphere()
{
    // segment constants need to stay in scope for the whole function
    static const unsigned int X_SEGMENTS = 32;
    static const unsigned int Y_SEGMENTS = 16;
    static unsigned int indexCount = 0; // will be filled once

    if (sphereVAO == 0)
    {
        std::vector<glm::vec3> positions;
        std::vector<unsigned int> indices;
        for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
        {
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
            {
                float xSegment = (float)x / X_SEGMENTS;
                float ySegment = (float)y / Y_SEGMENTS;
                float xPos = std::cos(xSegment * 2.0f * M_PI) * std::sin(ySegment * M_PI);
                float yPos = std::cos(ySegment * M_PI);
                float zPos = std::sin(xSegment * 2.0f * M_PI) * std::sin(ySegment * M_PI);
                positions.emplace_back(xPos, yPos, zPos);
            }
        }
        bool oddRow = false;
        for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
        {
            if (!oddRow) // even rows: y == 0, y == 2; and so on
            {
                for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
                {
                    indices.push_back(y       * (X_SEGMENTS + 1) + x);
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                }
            }
            else
            {
                for (int x = X_SEGMENTS; x >= 0; --x)
                {
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                    indices.push_back(y       * (X_SEGMENTS + 1) + x);
                }
            }
            oddRow = !oddRow;
        }

        glGenVertexArrays(1, &sphereVAO);
        glGenBuffers(1, &sphereVBO);
        glBindVertexArray(sphereVAO);
        glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
        glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3),
                     positions.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

        unsigned int EBO;
        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                     indices.data(), GL_STATIC_DRAW);

        indexCount = static_cast<unsigned int>(indices.size());

        glBindVertexArray(0);
    }

    glBindVertexArray(sphereVAO);
    // draw all indices generated earlier
    glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// render the scene with the given shader
void renderScene(const Shader &shader)
{
    // floor
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 7.0f, 0.0));
    model = glm::scale(model, glm::vec3(7.5f, 7.5f, 7.5f));
    shader.setMat4("model", model);
    shader.setInt("invertedNormals", 1);
    renderCube();
    shader.setInt("invertedNormals", 0);
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 0.5f, 0.0));
    model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0, 0.0, 0.0));
    model = glm::scale(model, glm::vec3(1.0f));
    shader.setMat4("model", model);
}
// process all input: query GLFW whether relevant keys are pressed/released this
// frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    const float cameraSpeed = 2.5f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Toggle mouse cursor for ImGui interaction with Tab key
    static bool mouseCaptured = true;
    static double lastTabPress = 0.0;
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS)
    {
        double currentTime = glfwGetTime();
        if (currentTime - lastTabPress > 0.5)
        { // Add delay to avoid multiple toggles
            mouseCaptured = !mouseCaptured;
            glfwSetInputMode(window, GLFW_CURSOR,
                             mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            lastTabPress = currentTime;
        }
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);

    // Toggle SSAO debug view with key 'O'
    static bool oPressedLastFrame = false;
    bool oPressedNow = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;
    if (oPressedNow && !oPressedLastFrame) {
        gShowSSAO = !gShowSSAO;
    }
    oPressedLastFrame = oPressedNow;
}

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// Zoom callback
// -------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// glfw: whenever the window size changed (by OS or user resize) this callback
// function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width
    // and height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

unsigned int loadTexture(char const *path, bool gammaCorrection)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum internalFormat;
        GLenum dataFormat;
        if (nrComponents == 1)
            internalFormat = dataFormat = GL_RED;
        else if (nrComponents == 3)
        {
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            dataFormat = GL_RGB;
        }
        else if (nrComponents == 4)
        {
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
            dataFormat = GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // for this tutorial: use GL_CLAMP_TO_EDGE to prevent semi-transparent borders. Due to interpolation it takes texels from next repeat
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

// ----------------------------------------------------------------------------
// Calculate and display FPS in the window title every ~0.25 s
// ----------------------------------------------------------------------------
void showFPS(GLFWwindow* window)
{
    static double previousSeconds = 0.0;
    static int frameCount = 0;

    double currentSeconds = glfwGetTime();
    double elapsedSeconds = currentSeconds - previousSeconds;

    // Update the title at most four times a second to avoid spamming
    if (elapsedSeconds > 0.25)
    {
        double fps = static_cast<double>(frameCount) / elapsedSeconds;

        std::stringstream ss;
        ss << "Lighting Example - " << std::fixed << std::setprecision(2) << fps << " FPS";

        glfwSetWindowTitle(window, ss.str().c_str());

        frameCount = 0;
        previousSeconds = currentSeconds;
    }

    frameCount++;
}