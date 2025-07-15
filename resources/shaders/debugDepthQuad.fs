#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform float exposure;

void main()
{
//     const float gamma = 2.2;
//     vec3 hdrColor = texture(scene, TexCoords).rgb;
//     vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
//     hdrColor += bloomColor;
//     vec3 result = vec3(1.0) - exp(-hdrColor * exposure);
//     // also gamma correct while we're at it
//     result = pow(result, vec3(1.0 / gamma));
    vec3 result = vec3(1.0, 0.0, 0.0);

    FragColor = vec4(result, 1.0);// Load a glTF/GLB file into the provided tinygltf::Model.
                                  // Returns true on success.
                                  bool loadModel(tinygltf::Model &model, const char *filename) {
                                      std::cout << "loadModel" << std::endl;
                                      tinygltf::TinyGLTF loader;
                                      std::string err;
                                      std::string warn;

                                      bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
                                      if (!warn.empty()) {
                                          std::cout << "WARN: " << warn << std::endl;
                                      }

                                      if (!err.empty()) {
                                          std::cout << "ERR: " << err << std::endl;
                                      }

                                      if (!res)
                                          std::cout << "Failed to load glTF: " << filename << std::endl;
                                      else
                                          std::cout << "Loaded glTF: " << filename << std::endl;

                                      return res;
                                  }

}