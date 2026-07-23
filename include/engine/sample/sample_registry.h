#ifndef ENGINE_SAMPLE_SAMPLE_REGISTRY_H
#define ENGINE_SAMPLE_SAMPLE_REGISTRY_H

#include <engine/sample/sample.h>

#include <memory>
#include <string>
#include <vector>

namespace engine::sample {

// Process-local factory registry. Factories are stored without being invoked and
// run only on create(), so listing cannot initialize a sample, GLFW, or GL.
// Malformed/duplicate registrations throw std::logic_error. Samples returned by
// create() are independently owned by the caller.
class SampleRegistry final {
public:
    void add(SampleRegistration registration);
    std::vector<SampleDescriptor> list() const;
    std::unique_ptr<Sample> create(const std::string& name) const;

private:
    struct Entry {
        SampleDescriptor descriptor;
        SampleFactory factory = nullptr;
    };

    std::vector<Entry> entries_;
};

SampleRegistry createDefaultRegistry();

}  // namespace engine::sample

#endif
