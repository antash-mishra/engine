//
// Created on 20/7/25.
//

#include <glad/glad.h>
#include <cstddef>
#include <GLFW/glfw3.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>

#include "shader.h"
#include "camera.h"
#include "stb_image.h"
#include "terrainGeneration.h"
#include <filesystem>
namespace fs = std::filesystem;

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
unsigned int loadTexture(const char *path, bool gammaCorrection = true);
void renderSphere();
void renderCube();
void renderQuad();
void showFPS(GLFWwindow* window);
// add the following prototype so it can be used before definition
void window_focus_callback(GLFWwindow* window, int focused);

// settings (initial)
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

unsigned int gWindowWidth  = SCR_WIDTH;
unsigned int gWindowHeight = SCR_HEIGHT;

// Time
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Mouse initial position
float lastX = SCR_WIDTH / 2.0;
float lastY = SCR_HEIGHT / 2.0;
bool firstMouse = true;
float fov = 45.0f;

unsigned int planeVAO;
unsigned int cubeVAO, cubeVBO = 0;

unsigned int quadVAO = 0;
unsigned int quadVBO;

unsigned int sphereVAO = 0, sphereVBO = 0;

// Better camera position for terrain viewing
Camera camera(glm::vec3(0.0f, 50.0f, 150.0f),
              glm::vec3(0.0f, 1.0f, 0.0f),
              -90.0f, -20.0f);


int useWireframe = 1;  // Start in wireframe mode for debugging
int displayGrayscale = 0;

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // glfwWindowHint(GLFW_SAMPLES, 4);

    // create window
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetWindowFocusCallback(window, window_focus_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    // glEnable(GL_MULTISAMPLE);
    // glCullFace(GL_BACK);

    std::string parentDir = (fs::current_path().fs::path::parent_path()).string();
    // Load heightmap
    unsigned int texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    int width, height, nChannels;
    unsigned char *data = stbi_load((parentDir + "/resources/heightmap2.png").c_str(), &width, &height, &nChannels, 0);
    std::cout << "Loaded image: " << (parentDir + "/resources/heightmap4.png") << " size: " << width << "x" << height << " channels: " << nChannels << std::endl;

    if (data) {
        GLenum format = GL_RGB;
        if (nChannels == 1)
        {
            format = GL_RED;
            // For single-channel textures with arbitrary width, rows are 1-byte aligned
            // so we must set unpack alignment to 1 (default is 4). Otherwise drivers may
            // read out-of-bounds when width is not a multiple of 4 (e.g. 8129).
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        }
        else if (nChannels == 3)
            format = GL_RGB;
        else if (nChannels == 4)
            format = GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        // Restore default unpack alignment
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }
    else {
        std::cout << "Failed to load image" << std::endl;
    }
    stbi_image_free(data);

    // Create TerrainGenerator instance (update the path to your compute shader as needed)
    // Use a safe, fixed grid size (e.g., 256) instead of image dimensions
    int gridSize = 1024;
    // Use reasonable world size instead of image dimensions
    int terrainWorldSize = 200;  // 200x200 world units instead of 2624x1700
    TerrainGenerator terrainGen((parentDir + "/resources/heightmap/heightmapCompute.glsl").c_str(), gridSize, terrainWorldSize, terrainWorldSize);
    terrainGen.generateTerrain(texture);

    // Set up your rendering shader (vertex/fragment)
    Shader renderShader((parentDir + "/resources/heightmap/heightMapVertex.glsl").c_str(), (parentDir + "/resources/heightmap/heightMapFragment.glsl").c_str());
    renderShader.use();
    renderShader.setInt("heightMap", 0);

    while (!glfwWindowShouldClose(window)) {
        showFPS(window);
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);
        
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Toggle wireframe mode based on useWireframe variable
        if (useWireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100000.0f);
        glm::mat4 view = camera.GetViewMatrix();
        renderShader.use();
        renderShader.setMat4("projection", projection);
        renderShader.setMat4("view", view);
        glm::mat4 model = glm::mat4(1.0f);
        renderShader.setMat4("model", model);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        terrainGen.render();
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
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
// process all input: query GLFW whether relevant keys are pressed/released this
// frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    const float cameraSpeed = 50.0f * deltaTime;  // Increased speed for the terrain scale

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
    
    // Toggle wireframe mode with F key
    static double lastFPress = 0.0;
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        double currentTime = glfwGetTime();
        if (currentTime - lastFPress > 0.5) {
            useWireframe = !useWireframe;
            lastFPress = currentTime;
        }
    }
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

// new: focus callback to automatically (re)capture the mouse cursor when the
// window gains focus and release it when it loses focus.
void window_focus_callback(GLFWwindow* window, int focused)
{
    if (focused)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        // Reset firstMouse to avoid sudden jump due to cursor recentering
        firstMouse = true;
    }
    else
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback
// function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width
    // and height will be significantly larger than specified on retina displays.
    gWindowWidth  = width;
    gWindowHeight = height;
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
