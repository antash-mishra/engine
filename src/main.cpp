#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>
#include <limits.h> // For PATH_MAX
#include <ostream>
#include <string>
#include <unistd.h> // For getcwd
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>

#include "glm/detail/func_trigonometric.hpp"
#include "glm/detail/type_vec.hpp"
#include "shader.h"
#include "stb_image.h"
#include "camera.h"
#include "model.h"
#include "filesystem.h"

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

// Time
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Mouse initial position
float lastX = SCR_WIDTH/2.0;
float lastY = SCR_HEIGHT/2.0;
bool firstMouse = true;
float fov = 45.0f;

// light pos
// glm::vec3 lightPos(0.5f, 0.0f, 2.0f);
glm::vec3 lightPos(1.2f, 1.0f, 2.0f);

int main() {

  // glfw: initialize and configure
  // ------------------------------
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(true);

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile shaders
    // -------------------------
    Shader ourShader("resources/shaders/vertexShader.vs", "resources/shaders/fragmentShader.fs");

    // load models
    // -----------
    std::string modelPath = FileSystem::getPath("objects/backpack/backpack.obj");
    std::cout << "Loading model from: " << modelPath << std::endl;
    
    // Debug the directory structure
    std::cout << "Checking if file exists..." << std::endl;
    std::ifstream f(modelPath.c_str());
    bool fileExists = f.good();
    f.close();
    std::cout << "File exists: " << (fileExists ? "Yes" : "No") << std::endl;
    
    // Model loading with error handling
    Model* ourModel = nullptr;
    try {
      std::cout << "About to create model object..." << std::endl;
      ourModel = new Model(modelPath, false);
      std::cout << "Model loaded successfully." << std::endl;
    }
    catch (const std::exception& e) {
      std::cerr << "ERROR: Failed to load model: " << e.what() << std::endl;
      glfwTerminate();
      return -1;
    }
    catch (...) {
      std::cerr << "ERROR: Unknown exception while loading model." << std::endl;
      glfwTerminate();
      return -1;
    }

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window)) {
      // input
      // -----
      processInput(window);

      float currentFrame = static_cast<float>(glfwGetTime());
      deltaTime = currentFrame - lastFrame;
      lastFrame = currentFrame;

      // render
      // ------
      glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      
      // object shader
      ourShader.use();
      
      // view/projection transformations
      glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
      glm::mat4 view = camera.GetViewMatrix();
      ourShader.setMat4("projection", projection);
      ourShader.setMat4("view", view);

      // render the loaded model
      glm::mat4 model = glm::mat4(1.0f);
      model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
      model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
      ourShader.setMat4("model", model);
      ourModel->Draw(ourShader);
      
      // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved
      // etc.)
      // -------------------------------------------------------------------------------
      glfwSwapBuffers(window);
      glfwPollEvents();
    }
    
    // Clean up
    if (ourModel != nullptr) {
      delete ourModel;
    }

  glfwTerminate();
  return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this
// frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
  
  const float cameraSpeed = 2.5f * deltaTime;
  
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.ProcessKeyboard(FORWARD, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.ProcessKeyboard(BACKWARD, deltaTime);
  if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.ProcessKeyboard(LEFT, deltaTime);
  if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.ProcessKeyboard(RIGHT, deltaTime);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {

  float xpos = static_cast<float>(xposIn);
  float ypos = static_cast<float>(yposIn);

  if(firstMouse) {
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
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
  camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// glfw: whenever the window size changed (by OS or user resize) this callback
// function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  // make sure the viewport matches the new window dimensions; note that width
  // and height will be significantly larger than specified on retina displays.
  glViewport(0, 0, width, height);
}
