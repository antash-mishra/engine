#ifndef ENGINE_SAMPLE_SAMPLE_H
#define ENGINE_SAMPLE_SAMPLE_H

#include <engine/assets/asset_root.h>

#include <memory>
#include <optional>
#include <string>

namespace engine::sample {

struct SampleDescriptor {
    std::string name;
    std::string summary;
};

struct SampleRunConfig {
    int width = 800;
    int height = 600;
    std::optional<unsigned int> frameLimit;
    assets::AssetRoot assetRoot;
};

struct SampleRunResult {
    unsigned int framesRendered = 0;
};

// A sample owns its rendering behavior but not launcher parsing or asset-root
// policy. run() is called once on the main thread. It returns only after all
// sample-owned GPU resources and its window have been destroyed, and throws
// std::runtime_error when initialization or rendering cannot complete.
class Sample {
public:
    virtual ~Sample() = default;
    virtual const SampleDescriptor& descriptor() const noexcept = 0;
    virtual SampleRunResult run(const SampleRunConfig& config) = 0;
};

using SampleFactory = std::unique_ptr<Sample> (*)();

// Keeps discovery metadata separate from construction. A registry can validate
// and list registrations without invoking sample code or initializing a platform.
struct SampleRegistration {
    SampleDescriptor descriptor;
    SampleFactory factory = nullptr;
};

}  // namespace engine::sample

#endif
