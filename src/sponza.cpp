//
// Created by antash on 9/7/25.
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
#include "legacy_baseline.h"
#include "stb_image.h"

#include <filesystem>
namespace fs = std::filesystem;

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "load_gltf.h"


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
float exposure = 1.5f;
float ambientScale = 0.6f;
bool enableSSAO = true; // Toggle for SSAO

Camera camera(glm::vec3(13.0f, 5.0f, 0.0f));

// Global loader for light cube toggle
GLTFLoader loader;

// Vertices coordinates
GLfloat vertices[] =
{
    // positions            // normals         // texcoords
    25.0f, -0.5f, 25.0f, 0.0f, 1.0f, 0.0f, 25.0f, 0.0f,
    -25.0f, -0.5f, 25.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
    -25.0f, -0.5f, -25.0f, 0.0f, 1.0f, 0.0f, 0.0f, 25.0f,

    25.0f, -0.5f, 25.0f, 0.0f, 1.0f, 0.0f, 25.0f, 0.0f,
    -25.0f, -0.5f, -25.0f, 0.0f, 1.0f, 0.0f, 0.0f, 25.0f,
    25.0f, -0.5f, -25.0f, 0.0f, 1.0f, 0.0f, 25.0f, 25.0f
};

unsigned int planeVAO;
unsigned int cubeVAO, cubeVBO = 0;

unsigned int quadVAO = 0;
unsigned int quadVBO;

unsigned int sphereVAO = 0, sphereVBO = 0;

void setupCubeMap(unsigned int& textureID, unsigned int width, unsigned int height, bool mipmap)
{
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (mipmap)
    {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// A specified integer generator keeps the SSAO kernel stable across standard
// library implementations. The high 24 bits map exactly into a float mantissa.
float nextBaselineRandom(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state >> 8) * (1.0f / 16777216.0f);
}

int main(int argc, char** argv) {
    const legacy_baseline::TimePoint processStart = legacy_baseline::Clock::now();
    legacy_baseline::Session baseline(argc, argv, "sponza", processStart);

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (baseline.enabled()) {
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    }

    // create window
    const unsigned int renderWidth =
        baseline.enabled() ? baseline.config().width : SCR_WIDTH;
    const unsigned int renderHeight =
        baseline.enabled() ? baseline.config().height : SCR_HEIGHT;
    GLFWwindow *window = glfwCreateWindow(
        renderWidth, renderHeight, "Lighting Example", nullptr, nullptr);
    if (window == NULL)
    {
        std::cout << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    if (!baseline.enabled()) {
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetScrollCallback(window, scroll_callback);
        // Capture/release cursor automatically when window focus changes.
        glfwSetWindowFocusCallback(window, window_focus_callback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSwapInterval(0);
        gWindowWidth = renderWidth;
        gWindowHeight = renderHeight;
    }

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    baseline.installGlDiagnostics();
    baseline.addStartupSpan(
        "window_context_glad", processStart, legacy_baseline::Clock::now());

    if (baseline.enabled()) {
        const auto& settings = baseline.config().camera;
        camera.Position = glm::vec3(
            settings.position[0], settings.position[1], settings.position[2]);
        camera.Front = glm::normalize(glm::vec3(
            settings.forward[0], settings.forward[1], settings.forward[2]));
        camera.WorldUp = glm::normalize(glm::vec3(
            settings.up[0], settings.up[1], settings.up[2]));
        camera.Right = glm::normalize(glm::cross(camera.Front, camera.WorldUp));
        camera.Up = glm::normalize(glm::cross(camera.Right, camera.Front));
        camera.Zoom = settings.verticalFovDegrees;
        camera.near = settings.nearPlane;
        camera.far = settings.farPlane;
    } else {
        camera.near = 0.5f;
        camera.far = 60.0f;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL); // set depth function to less than AND equal for skybox depth trick.
    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);


    const fs::path resourceRoot = baseline.enabled()
        ? baseline.config().resourceRoot
        : fs::current_path().parent_path() / "resources";
    const legacy_baseline::TimePoint shaderStart = legacy_baseline::Clock::now();
    // Shader shader(
    //     (resourceRoot / "shaders/debugDepthQuad.vs").c_str(),
    //     (resourceRoot / "shaders/debugDepthQuad.fs").c_str());
    Shader modelSponza (
        (resourceRoot / "sponza/modelSponza.vs").c_str(),
        (resourceRoot / "sponza/modelSponza.fs").c_str());
    Shader equirectangularToCubemapShader((resourceRoot / "shaders/cube.vs").c_str(), (resourceRoot / "shaders/cube.fs").c_str());
    // Shader proceduralSkyShader((resourceRoot / "shaders/cube.vs").c_str(), (resourceRoot / "shaders/cube.fs").c_str());
    Shader depthShader((resourceRoot / "sponza/depthMap.vs").c_str(), (resourceRoot / "sponza/depthMap.fs").c_str());
    Shader debugDepthQuadShader((resourceRoot / "sponza/debugDepthQuad.vs").c_str(), (resourceRoot / "sponza/debugDepthQuad.fs").c_str());
    Shader cubeMapShader((resourceRoot / "shaders/background.vs").c_str(), (resourceRoot / "shaders/background.fs").c_str());
    Shader irradianceShader((resourceRoot / "shaders/cube.vs").c_str(), (resourceRoot / "shaders/irradiance.fs").c_str());
    Shader prefilterShader((resourceRoot / "shaders/cube.vs").c_str(), (resourceRoot / "shaders/prefilter.fs").c_str());
    Shader brdfShader((resourceRoot / "shaders/brdf.vs").c_str(), (resourceRoot / "shaders/brdf.fs").c_str());
    Shader geometryPassShader((resourceRoot / "sponza/gPass.vs").c_str(), (resourceRoot / "sponza/gPass.fs").c_str());
    Shader ssao((resourceRoot / "sponza/ssao.vs").c_str(), (resourceRoot / "sponza/ssao.fs").c_str());
    Shader ssaoBlur((resourceRoot / "sponza/ssao.vs").c_str(),   (resourceRoot / "sponza/ssaoBlur.fs").c_str());
    baseline.addStartupSpan(
        "shader_creation", shaderStart, legacy_baseline::Clock::now());

    // Use GLTFLoader class
    // GLTFLoader loader; // This line is moved to global scope
    const fs::path gltfPath =
        resourceRoot / "main-sponza/main_sponza/NewSponza_Main_glTF_003.gltf";
    const legacy_baseline::TimePoint modelStart = legacy_baseline::Clock::now();
    if (!loader.loadModel(gltfPath.string())) {
        std::cout << "Failed to load model" << std::endl;
        return -1;
    }
    baseline.addStartupSpan(
        "gltf_load_and_gpu_upload", modelStart, legacy_baseline::Clock::now());
    std::uint64_t retainedCpuLowerBound = 0;
    for (const tinygltf::Buffer& buffer : loader.model.buffers) {
        retainedCpuLowerBound += buffer.data.size();
    }
    for (const tinygltf::Image& image : loader.model.images) {
        retainedCpuLowerBound += image.image.size();
    }
    for (const Mesh& mesh : loader.meshes) {
        retainedCpuLowerBound += mesh.vertices.size() * sizeof(Vertex);
        retainedCpuLowerBound += mesh.indices.size() * sizeof(unsigned int);
    }
    baseline.setRetainedCpuBytes(
        retainedCpuLowerBound,
        "Lower bound: live TinyGLTF buffer/image payload sizes plus GLTFLoader CPU mesh "
        "vertex/index sizes; excludes vector capacity, object overhead, allocator slack, and GPU memory");

    const legacy_baseline::TimePoint sceneSetupStart = legacy_baseline::Clock::now();
    stbi_set_flip_vertically_on_load(true);
    int width, height, nrComponents;
    // Load the .hdr file
    float *data = stbi_loadf((resourceRoot / "w.hdr").c_str(), &width, &height, &nrComponents, 0);
    unsigned int hdrTexture;
    if (data) {
      glGenTextures(1, &hdrTexture);
      glBindTexture(GL_TEXTURE_2D, hdrTexture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGB, GL_FLOAT, data);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      stbi_image_free(data);
    }
    else {
      std::cout << "Failed to load HDR texture" << std::endl;
    }

    // Generating random sample kernel with 64 sample values
    std::uint32_t randomState = baseline.enabled() ? baseline.config().seed : 1u;
    std::vector<glm::vec3> ssaoKernel;
    for (unsigned int i=0; i<64; ++i) {
        glm::vec3 sample(
            nextBaselineRandom(randomState) * 2.0f - 1.0f,
            nextBaselineRandom(randomState) * 2.0f - 1.0f,
            nextBaselineRandom(randomState)
        );
        sample = glm::normalize(sample);
        sample *= nextBaselineRandom(randomState);
        float scale = (float)i / 64.0f;
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sample);
    }

    // Generating noise Texture
    // used to rotate sample kernel
    std::vector<glm::vec4> ssaoNoise;
    for (unsigned int i = 0; i < 25; ++i) {
        glm::vec4 noise(
            nextBaselineRandom(randomState) * 2.0f - 1.0f,
            nextBaselineRandom(randomState) * 2.0f - 1.0f,
            0.0f,
            1.0f
        );
        ssaoNoise.push_back(noise);
    }

    // Geometry pass
    unsigned int gBuffer;
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

    // position color buffer;
    unsigned int gPosition, gNormal;
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, renderWidth, renderHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

    // normal color buffer;
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, renderWidth, renderHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

    constexpr unsigned int attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);

    // Add rbo as depth buffer to check completeness
    unsigned int grboDepth;
    glGenRenderbuffers(1, &grboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, grboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, renderWidth, renderHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, grboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Kernel Noise (Rotation) Buffer (5×5 tileable noise)
    // -----------------------------------------------------
    const unsigned int noiseDim = 5;
    unsigned int noiseTexture;
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    // Store as RGB; alpha channel not needed
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, noiseDim, noiseDim, 0, GL_RGBA, GL_FLOAT, ssaoNoise.data());

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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, renderWidth, renderHeight, 0, GL_RED, GL_FLOAT, NULL);
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, renderWidth, renderHeight, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBufferBlur, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "SSAO Blur Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);



    // framebuffer for shadow depth map
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);

    const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024 ;
    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT,0,GL_DEPTH_COMPONENT,GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // attach depth map texture to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE); // No color buffer is drawn to
    glReadBuffer(GL_NONE); // No color buffer is read from
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // capture fbo and rbo for indirect lighting
    unsigned int captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    // cube map
    unsigned int envCubeMap;
    setupCubeMap(envCubeMap, 512, 512, false);


    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };
    equirectangularToCubemapShader.use();
    equirectangularToCubemapShader.setInt("equirectangularMap", 0);
    equirectangularToCubemapShader.setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    glViewport(0, 0, 512, 512);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    for (unsigned int i = 0; i < 6; ++i) {
        equirectangularToCubemapShader.setMat4("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubeMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // remove artifact
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubeMap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    unsigned int irradianceMap;
    setupCubeMap(irradianceMap, 32, 32, false);

    irradianceShader.use();
    irradianceShader.setInt("environmentMap", 0);
    irradianceShader.setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubeMap);

    glViewport(0, 0, 32, 32);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; i++) {
        irradianceShader.setMat4("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // pre-filter map
    unsigned int preFilterMap;
    setupCubeMap(preFilterMap, 128, 128, true);


    prefilterShader.use();
    prefilterShader.setInt("environmentMap", 0);
    prefilterShader.setMat4("projection", captureProjection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubeMap);

    // glViewport(0, 0, 128, 128);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip) {
        // resize frame buffer mip-size
        unsigned int mipWidth = 128 * std::pow(0.5, mip);
        unsigned int mipHeight = 128 * std::pow(0.5, mip);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = (float)mip / (float)(maxMipLevels - 1);
        prefilterShader.setFloat("roughness", roughness);
        for (unsigned int i=0; i<6; ++i) {
            prefilterShader.setMat4("view", captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, preFilterMap, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderCube();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // BRDF Convolution Texture
    unsigned int brdfLUTTexture;
    glGenTextures(1, &brdfLUTTexture);

    // pre-allocate memory for lut texture
    glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // reuse same framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);
    // render quad and viewport
    glViewport(0, 0, 512, 512);
    brdfShader.use();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderQuad();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ------------------------------------------------------------
    //   Extract first enabled directional light for shadow mapping
    // ------------------------------------------------------------
    glm::mat4 lightTransform;
    if (!loader.getFirstDirectionalLightTransform(lightTransform)) {
        std::cerr << "No active directional light found. Shadows disabled." << std::endl;
        // Fallback to default transform pointing down
        lightTransform = glm::mat4(1.0f);
        lightTransform = glm::translate(lightTransform, glm::vec3(0.0f, 10.0f, 0.0f));
        lightTransform = glm::rotate(lightTransform, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    // Keep the transform for reuse each frame
    glm::mat4 cachedLightTransform = lightTransform;

    // Pre-compute light direction (−Z axis transformed by rotation part)
    glm::vec3 cachedLightDir = glm::normalize(glm::mat3(cachedLightTransform) * glm::vec3(0.0f, 0.0f, -1.0f));

    // Define scene bounds (adjust if your scene is offset/ larger)
    glm::vec3 sceneCenter(0.0f);     // world-space centre of the scene
    const float sceneRadius = 20.0f; // encompasses entire Sponza atrium

    // Position the virtual light camera far enough to encapsulate the scene
    glm::vec3 cachedLightPos = sceneCenter - cachedLightDir * sceneRadius;

    modelSponza.use();
    modelSponza.setInt("shadowMap", 2);
    modelSponza.setInt("irradianceMap", 5);
    modelSponza.setInt("brdfLUT", 6);
    modelSponza.setInt("prefilterMap", 7);
    modelSponza.setInt("ssaoMap", 8);
    modelSponza.setFloat("exposure", exposure);
    modelSponza.setFloat("ambientScale", ambientScale);
    loader.setupLighting(modelSponza);


    debugDepthQuadShader.use();
    debugDepthQuadShader.setInt("depthMap", 0);

    ssao.use();
    ssao.setInt("gPosition", 0);
    ssao.setInt("gNormal", 1);
    ssao.setInt("texNoise", 2);

    ssaoBlur.use();
    ssaoBlur.setInt("ssaoInput", 0);

    cubeMapShader.use();
    cubeMapShader.setInt("envCubeMap", 0);

    // then before rendering, configure the viewport to the original framebuffer's screen dimensions
    int scrWidth, scrHeight;
    glfwGetFramebufferSize(window, &scrWidth, &scrHeight);
    glViewport(0, 0, scrWidth, scrHeight);
    const bool framebufferExtentMatches =
        !baseline.enabled() ||
        (scrWidth == static_cast<int>(renderWidth) &&
         scrHeight == static_cast<int>(renderHeight));
    baseline.addStartupSpan(
        "scene_targets_and_ibl", sceneSetupStart, legacy_baseline::Clock::now());

    while (!glfwWindowShouldClose(window)) {
        const legacy_baseline::TimePoint frameStart = baseline.beginFrame();
        // Update window title with FPS information
        if (!baseline.enabled()) {
            showFPS(window);
        }

        // calculate delta Time
        float currentFrame = baseline.enabled()
            ? static_cast<float>(baseline.fixedTimeSeconds())
            : static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (!baseline.enabled()) {
            processInput(window);
        }

        // Clear the screen
        glClearColor(0.9999999, 0.9999998, 1.0, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);



        // render scene with depth map
        glm::mat4 viewMatrix = camera.GetViewMatrix();
        glm::mat4 projectionMatrix = glm::perspective(glm::radians(camera.Zoom), (float)gWindowWidth / (float)gWindowHeight, camera.near, camera.far);

        // Geometry pass
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
        glDepthMask(GL_TRUE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        geometryPassShader.use();
        geometryPassShader.setMat4("view", viewMatrix);
        loader.Render(geometryPassShader, viewMatrix, projectionMatrix);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // SSAO pass (only if enabled)
        if (enableSSAO) {
            glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ssao.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, gPosition);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, gNormal);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, noiseTexture);
            for (unsigned int i=0; i<ssaoKernel.size(); i++) {
                ssao.setVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
            }
            ssao.setMat4("projection", projectionMatrix);
            ssao.setVec2(
                "noiseScale",
                glm::vec2(renderWidth / 5.0f, renderHeight / 5.0f));
            renderQuad();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Ambient Occlusion blur to remove the repeated pattern
            // -----------------------------------------------------
            glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
            glClear(GL_COLOR_BUFFER_BIT);
            ssaoBlur.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
            renderQuad();
            glDepthMask(GL_TRUE);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        // ------------------------------------------------------------------
        // Depth pass
        // ------------------------------------------------------------------
        depthShader.use();
        const float near_plane = 1.0f;
        const float far_plane  = sceneRadius * 2.0f;  // cover entire scene depth
        const float orthoHalf  = sceneRadius;         // half-width of ortho box

        // Orthographic projection that encloses the scene bounds
        glm::mat4 lightProjection = glm::ortho(-orthoHalf, orthoHalf,
                                               -orthoHalf, orthoHalf,
                                               near_plane, far_plane);

        // Build view matrix from the light's position & direction
        glm::mat4 lightView = glm::lookAt(cachedLightPos,
                                          sceneCenter,
                                          glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glCullFace(GL_FRONT);
        loader.Render(depthShader, lightView, lightProjection);
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // reset viewport
        glViewport(0, 0, gWindowWidth, gWindowHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);




        modelSponza.use();
        modelSponza.setFloat("exposure", exposure);
        modelSponza.setFloat("ambientScale", ambientScale);

        // Set camera position for specular lighting (must be after shader.use())
        // modelSponza.setVec3("viewPos", camera.Position);
        modelSponza.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, depthMap);   // depthMap was generated earlier
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_CUBE_MAP, preFilterMap);
        glActiveTexture(GL_TEXTURE8);
        // Bind SSAO texture (use white texture if disabled)
        if (enableSSAO) {
            glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
        } else {
            // Create a white texture for when SSAO is disabled
            static unsigned int whiteTexture = 0;
            if (whiteTexture == 0) {
                glGenTextures(1, &whiteTexture);
                glBindTexture(GL_TEXTURE_2D, whiteTexture);
                float whitePixel[] = {1.0f, 1.0f, 1.0f, 1.0f};
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_FLOAT, whitePixel);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            }
            glBindTexture(GL_TEXTURE_2D, whiteTexture);
        }
        modelSponza.setVec3("camPosition", camera.Position);
        loader.Render(modelSponza, viewMatrix, projectionMatrix);


        // Before skybox rendering:
        // glDepthFunc(GL_LEQUAL);
        // glDisable(GL_CULL_FACE);
        // render skybox (render as last to prevent overdraw)
        cubeMapShader.use();

        // Pass the full view matrix - the shader will extract rotation internally
        cubeMapShader.setMat4("view", viewMatrix);
        cubeMapShader.setMat4("projection", projectionMatrix);
        glActiveTexture(GL_TEXTURE0);

        glBindTexture(GL_TEXTURE_CUBE_MAP, envCubeMap);
        glDepthFunc(GL_LEQUAL); // change depth function so depth test passes when values are equal to depth buffer's content
        renderCube();
        // After skybox rendering:
        glDepthFunc(GL_LESS);

        baseline.endCpuFrame(frameStart);
        baseline.markFirstFrameGpuComplete();
        if (baseline.isFinalCaptureFrame()) {
            baseline.captureDefaultColor(
                "final_color",
                static_cast<int>(gWindowWidth),
                static_cast<int>(gWindowHeight));
            baseline.captureTextureFloat(
                "view_position",
                gPosition,
                static_cast<int>(renderWidth),
                static_cast<int>(renderHeight),
                GL_RGBA,
                4,
                "view_space_position");
            baseline.captureTextureFloat(
                "view_normal",
                gNormal,
                static_cast<int>(renderWidth),
                static_cast<int>(renderHeight),
                GL_RGBA,
                4,
                "view_space_normal");
            baseline.captureTextureFloat(
                "ssao",
                ssaoColorBufferBlur,
                static_cast<int>(renderWidth),
                static_cast<int>(renderHeight),
                GL_RED,
                1,
                "ambient_visibility");
            baseline.captureTextureFloat(
                "shadow_depth",
                depthMap,
                static_cast<int>(SHADOW_WIDTH),
                static_cast<int>(SHADOW_HEIGHT),
                GL_DEPTH_COMPONENT,
                1,
                "normalized_depth");
        }


        // render Depth map to quad for visual debugging
        // ---------------------------------------------
        /*
        debugDepthQuadShader.use();
        debugDepthQuadShader.setFloat("near_plane", near_plane);
        debugDepthQuadShader.setFloat("far_plane", far_plane);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthMap);

        renderQuad();
        */

        glfwSwapBuffers(window);
        glfwPollEvents();
        if (baseline.advanceFrame()) {
            glfwSetWindowShouldClose(window, true);
        }
    }

    // Clean up VAOs if needed (GLTFLoader manages them)
    const int result = baseline.finish(framebufferExtentMatches ? 0 : 1);
    glfwDestroyWindow(window);
    // Terminate GLFW, clearing any resources allocated by GLFW.
    glfwTerminate();
    return result;
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

    // Exposure control with '=' and '-' keys
    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) {
        exposure = std::min(exposure + 0.1f * deltaTime, 5.0f); // Increase with upper limit
        std::cout << "Exposure: " << std::fixed << std::setprecision(2) << exposure << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) {
        exposure = std::max(exposure - 0.1f * deltaTime, 0.1f); // Decrease with lower limit
        std::cout << "Exposure: " << std::fixed << std::setprecision(2) << exposure << std::endl;
    }

    // Toggle light cube visualization with 'L' key
    static bool lPressedLastFrame = false;
    bool lPressedNow = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    if (lPressedNow && !lPressedLastFrame) {
        loader.showLightCubes = !loader.showLightCubes;
        std::cout << "Light cubes " << (loader.showLightCubes ? "enabled" : "disabled") << std::endl;
    }
    lPressedLastFrame = lPressedNow;

    // Toggle SSAO with key 'O'
    static bool oPressedLastFrame = false;
    bool oPressedNow = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;
    if (oPressedNow && !oPressedLastFrame) {
        enableSSAO = !enableSSAO;
        std::cout << "SSAO " << (enableSSAO ? "enabled" : "disabled") << std::endl;
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
