#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include <shader.h>
#include <camera.h>
#include <stb_image.h>

#include <filesystem>
namespace fs = std::filesystem;

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
unsigned int loadTexture(const char *path, bool gammaCorrection = true);
void renderQuad();
void showFPS(GLFWwindow* window);
void window_focus_callback(GLFWwindow* window, int focused);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

unsigned int gWindowWidth = SCR_WIDTH;
unsigned int gWindowHeight = SCR_HEIGHT;

// time
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Mouse position
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float fov = 45.0f;

Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));

unsigned int quadVAO = 0;
unsigned int quadVBO;

float cubeVertices[] = {
    // Face 1 (Front, -Z), Corrected to CCW
    // Triangle 1: Original V0, V1, V2 -> Corrected V0, V2, V1
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, // Original V0
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f, // Original V2
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f, // Original V1
    // Triangle 2: Original V3, V4, V5 -> Corrected V3, V5, V4
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f, // Original V3
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, // Original V5
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, // Original V4

    // Face 2 (Back, +Z), Already CCW - NO CHANGE
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

    // Face 3 (Left, -X), Already CCW - NO CHANGE
    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

    // Face 4 (Right, +X), Corrected to CCW
    // Triangle 1: Original V18, V19, V20 -> Corrected V18, V20, V19
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f, // Original V18
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f, // Original V20
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f, // Original V19
    // Triangle 2: Original V21, V22, V23 -> Corrected V21, V23, V22
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f, // Original V21
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f, // Original V23
     0.5f, -0.5f,  0.5f,  0.0f, 0.0f, // Original V22

    // Face 5 (Bottom, -Y), Already CCW - NO CHANGE
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

    // Face 6 (Top, +Y), Corrected to CCW
    // Triangle 1: Original V30, V31, V32 -> Corrected V30, V32, V31
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, // Original V30
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f, // Original V32
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f, // Original V31
    // Triangle 2: Original V33, V34, V35 -> Corrected V33, V35, V34
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f, // Original V33
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, // Original V35
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f  // Original V34
};

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // create window
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Ray Marcher", NULL, NULL);
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

    // load glad
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    std::string parentDir = (fs::current_path().fs::path::parent_path()).string();
    Shader shader((parentDir + "/resources/shaders/vertexCube.glsl").c_str(),
        (parentDir + "/resources/shaders/fragmentCube.glsl").c_str());

    GLuint cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

    renderQuad();

    while (!glfwWindowShouldClose(window)) {
        showFPS(window);
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
            static_cast<float>(SCR_WIDTH)/static_cast<float>(SCR_HEIGHT), 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        shader.use();
        shader.setMat4("projection", projection);
        shader.setVec3("cameraPosition", camera.Position);
        shader.setMat4("view", view);
        shader.setMat4("model", glm::mat4(1.0f));
        shader.setFloat("iTime", currentFrame);
        shader.setVec2("iResolution", SCR_WIDTH, SCR_HEIGHT);
        renderQuad();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
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