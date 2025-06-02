#include <filesystem>
namespace fs = std::filesystem;

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

// ImGui includes
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "glm/detail/func_trigonometric.hpp"
#include "glm/detail/type_vec.hpp"
#include "shader.h"
#include "stb_image.h"
#include "camera.h"
#include "model.h"

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
unsigned int loadCubeMap(vector<std::string> faces);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

// Time
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Mouse initial position
float lastX = SCR_WIDTH / 2.0;
float lastY = SCR_HEIGHT / 2.0;
bool firstMouse = true;
float fov = 45.0f;

// Model control variables
glm::vec3 modelPosition(0.0f, 0.0f, 0.0f);
glm::vec3 modelRotation(0.0f, 0.0f, 0.0f);
float modelScale = 0.5f;
glm::vec3 clearColor(0.1f, 0.1f, 0.1f);

// light pos
// glm::vec3 lightPos(0.5f, 0.0f, 2.0f);
glm::vec3 lightPos(1.2f, 1.0f, 2.0f);

// Vertices coordinates
GLfloat vertices[] =
    { //     COORDINATES    /    TexCoord
        -1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, -1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, -1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f, 0.0f};


GLfloat grassVertices[] =
    { //     COORDINATES    /    TexCoord
        0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
        0.0f, -0.5f,  0.0f,  0.0f,  1.0f,
        1.0f, -0.5f,  0.0f,  1.0f,  1.0f,

        0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
        1.0f, -0.5f,  0.0f,  1.0f,  1.0f,
        1.0f,  0.5f,  0.0f,  1.0f,  0.0f
    };

// Indices for vertices order
GLuint indices[] =
    {
        0, 1, 2,
        0, 2, 3
    };

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

float quadVertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
    // positions   // texCoords
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
};

float skyboxVertices[] = {
    // positions          
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};



int main()
{

  // glfw: initialize and configure
  // ------------------------------
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // glfw window creation
  // --------------------
  GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
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

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

  // Setup Platform/Renderer backends
  const char *glsl_version = "#version 330";
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // add grass translate position
  vector<glm::vec3> vegetation;
  vegetation.push_back(glm::vec3(-1.5f,  0.0f, -0.48f));
  vegetation.push_back(glm::vec3( 1.5f,  0.0f,  0.51f));
  vegetation.push_back(glm::vec3( 0.0f,  0.0f,  0.7f));
  vegetation.push_back(glm::vec3(-0.3f,  0.0f, -2.3f));
  vegetation.push_back(glm::vec3( 0.5f,  0.0f, -0.6f));


  vector<glm::vec3> windows
  {
      glm::vec3(-1.5f, 0.0f, -0.48f),
      glm::vec3( 1.5f, 0.0f, 0.51f),
      glm::vec3( 0.0f, 0.0f, 0.7f),
      glm::vec3(-0.3f, 0.0f, -2.3f),
      glm::vec3( 0.5f, 0.0f, -0.6f)
  };


  // configure global opengl state
  // -----------------------------
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  // glEnable(GL_STENCIL_TEST);
  // glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
  // glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE); 
  // glEnable(GL_BLEND);
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  // glFrontFace(GL_CCW);

  // build and compile shaders
  // -------------------------
  Shader ourShader("resources/shaders/vertexShader.vs", "resources/shaders/fragmentShader.fs");
  // Shader outlineShader("resources/shaders/vertexShader.vs", "resources/shaders/outlineShader.fs");
  Shader screenShader("resources/shaders/framebuffer.vs", "resources/shaders/framebuffer.fs");
  Shader skyboxShader("resources/shaders/skybox.vs", "resources/shaders/skybox.fs");

  // VAO, VBO and EBO for plane geometry  
  GLuint planeVAO, planeVBO, planeEBO, cubeVAO, cubeVBO;
  glGenVertexArrays(1, &planeVAO);
  glGenBuffers(1, &planeVBO);
  glGenBuffers(1, &planeEBO);

  glBindVertexArray(planeVAO);
  glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planeEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  // Position attribute
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid *)0);

  // Color attribute
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid *)(3 * sizeof(GLfloat)));

  // unbind the VAO and VBO
  glBindVertexArray(0);

  // Cube set VAO and VBO
  glGenVertexArrays(1, &cubeVAO);
  glGenBuffers(1, &cubeVBO);

  glBindVertexArray(cubeVAO);
  glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

  // Position attribute
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid *)0);

  // Texture attribute
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid *)(3 * sizeof(GLfloat)));

  // unbind the VAO and VBO
  glBindVertexArray(0);

  // Cube map VAO
  unsigned int skyboxVAO, skyboxVBO;
  glGenVertexArrays(1, &skyboxVAO);
  glGenBuffers(1, &skyboxVBO);
  glBindVertexArray(skyboxVAO);
  glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

  // screen quad VAO
  unsigned int quadVAO, quadVBO;
  glGenVertexArrays(1, &quadVAO);
  glGenBuffers(1, &quadVBO);
  glBindVertexArray(quadVAO);
  glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));


  // Load texture for plane
  // ----------------------
  unsigned int material_diffuse0;
  glGenTextures(1, &material_diffuse0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, material_diffuse0);

  // Set texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  std::string parentDir = (fs::current_path().fs::path::parent_path()).string();
  std::cout << "Parent directory: " << parentDir << std::endl;

  // Load image, create diffuse texture and generate mipmaps
  int width, height, nrChannels;
  stbi_set_flip_vertically_on_load(true); // tell stb_image.h to flip loaded texture's on the y-axis.
  
  unsigned char *data = stbi_load((parentDir + "/resources/metal.png").c_str(), &width, &height, &nrChannels, 0);

  if (data)
  {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    std::cout << "Texture loaded successfully" << std::endl;
  }
  else
  {
    std::cout << "Failed to load texture" << std::endl;
  }

  // Free image data
  stbi_image_free(data);


  // Texture for cube
  unsigned int cubeTexture;
  glGenTextures(1, &cubeTexture);
  glBindTexture(GL_TEXTURE_2D, cubeTexture);

  // Set texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Load image, create diffuse texture and generate mipmaps
  data = stbi_load((parentDir + "/resources/marble.jpg").c_str(), &width, &height, &nrChannels, 0);

  if (data)
  {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    std::cout << "Texture loaded successfully" << std::endl;
  }
  else
  {
    std::cout << "Failed to load texture" << std::endl;
  }

  // Free image data
  stbi_image_free(data);

  // Load cube map
  stbi_set_flip_vertically_on_load(false);
  vector<std::string> faces
  {
    parentDir + "/resources/skybox/right.jpg",
    parentDir + "/resources/skybox/left.jpg",
    parentDir + "/resources/skybox/top.jpg",
    parentDir + "/resources/skybox/bottom.jpg",
    parentDir + "/resources/skybox/front.jpg",
    parentDir + "/resources/skybox/back.jpg"
  };
  unsigned int cubemapTexture = loadCubeMap(faces);



  // Framebuffer
  // ------------
  unsigned int fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  // create a texture object for the framebuffer
  unsigned int textureColorbuffer;
  glGenTextures(1, &textureColorbuffer);
  glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // attach it to currently bound framebuffer object
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

  // create a renderbuffer object for depth and stencil attachment
  unsigned int rbo;
  glGenRenderbuffers(1, &rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);


  // shader configuration
  // --------------------
  ourShader.use();
  ourShader.setInt("texture_diffuse0", 0);

  screenShader.use();
  screenShader.setInt("screenTexture", 0);

  skyboxShader.use();
  skyboxShader.setInt("skybox", 0);

  // render loop
  // -----------
  while (!glfwWindowShouldClose(window))
  {
    // input
    // -----
    processInput(window);

    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    // render
    // ------
    // bind frame buffer object
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glEnable(GL_DEPTH_TEST);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ourShader.use();

    //  glStencilMask(0x00);
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    // view/projection transformations
    ourShader.setMat4("projection", projection);
    ourShader.setMat4("view", view);
    
    // Render plane
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(3.0f, 3.0f, 3.0f));
    ourShader.setMat4("model", model);
    
    // Actually render the plane
    glDisable(GL_CULL_FACE);  // Disable culling so plane is visible from both sides
    glBindVertexArray(planeVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, material_diffuse0);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glEnable(GL_CULL_FACE);   // Re-enable culling for other objects

    // Render cube
    glBindVertexArray(cubeVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cubeTexture);
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-1.0f, 0.0f, 0.0f));
    ourShader.setMat4("model", model);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(1.0f, 0.0f, 0.0f));
    ourShader.setMat4("model", model);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    
    // Add a test cube directly in front of camera
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, -2.0f));
    model = glm::scale(model, glm::vec3(0.5f, 0.5f, 0.5f));
    ourShader.setMat4("model", model);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glDepthFunc(GL_LEQUAL);
    skyboxShader.use();
    projection = glm::perspective(glm::radians(90.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    view = glm::mat4(glm::mat3(camera.GetViewMatrix()));
    skyboxShader.setMat4("projection", projection);
    skyboxShader.setMat4("view", view);
    glBindVertexArray(skyboxVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
    
    // now bind back to default framebuffer and draw a quad plane with the attached framebuffer color texture
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
    // clear all relevant buffers
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessary actually, since we won't be able to see behind the quad anyways)
    glClear(GL_COLOR_BUFFER_BIT);

    screenShader.use();
    glBindVertexArray(quadVAO);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);	// use the color attachment texture as the texture of the quad plane
    glDrawArrays(GL_TRIANGLES, 0, 6);


    
    

    // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved
    // etc.)
    // -------------------------------------------------------------------------------
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // Cleanup plane geometry
  glDeleteVertexArrays(1, &planeVAO);
  glDeleteBuffers(1, &planeVBO);
  glDeleteBuffers(1, &planeEBO);
  glDeleteVertexArrays(1, &quadVAO);
  glDeleteBuffers(1, &quadVBO);
  glDeleteVertexArrays(1, &skyboxVAO);
  glDeleteBuffers(1, &skyboxVBO);
  glDeleteTextures(1, &material_diffuse0);
  glDeleteVertexArrays(1, &cubeVAO);
  glDeleteBuffers(1, &cubeVBO);
  glDeleteTextures(1, &cubeTexture);
  glDeleteTextures(1, &textureColorbuffer);
  glDeleteRenderbuffers(1, &rbo);
  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(1, &cubemapTexture);
  glfwTerminate();
  return 0;
}

unsigned int loadCubeMap(vector<std::string> faces) {
  unsigned int textureID;
  glGenTextures(1, &textureID);

  glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

  int width, height, nrChannels;
  for (unsigned int i = 0; i < faces.size(); i++) {
    unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
    if (data) {
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
      stbi_image_free(data);
    } else {
      std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
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
  // Skip camera movement when ImGui wants to capture mouse
  ImGuiIO &io = ImGui::GetIO();
  if (io.WantCaptureMouse)
    return;

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
