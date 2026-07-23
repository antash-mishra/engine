#include <engine/sample/sample_registry.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

int factoryCalls = 0;

class StubSample final : public engine::sample::Sample {
public:
    explicit StubSample(engine::sample::SampleDescriptor descriptor)
        : descriptor_(std::move(descriptor)) {}

    const engine::sample::SampleDescriptor& descriptor() const noexcept override {
        return descriptor_;
    }

    engine::sample::SampleRunResult run(
        const engine::sample::SampleRunConfig&) override {
        return {};
    }

private:
    engine::sample::SampleDescriptor descriptor_;
};

std::unique_ptr<engine::sample::Sample> makeAlpha() {
    ++factoryCalls;
    return std::make_unique<StubSample>(
        engine::sample::SampleDescriptor{"alpha", "Alpha sample"});
}

std::unique_ptr<engine::sample::Sample> makeNull() {
    ++factoryCalls;
    return nullptr;
}

std::unique_ptr<engine::sample::Sample> makeMismatch() {
    ++factoryCalls;
    return std::make_unique<StubSample>(
        engine::sample::SampleDescriptor{"different", "Different sample"});
}

template <typename Function>
bool throwsLogicError(Function&& function) {
    try {
        function();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

bool require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "sample registry test failed: " << message << '\n';
    }
    return condition;
}

}  // namespace

int main() {
    bool passed = true;

    engine::sample::SampleRegistry registry;
    registry.add({{"alpha", "Alpha sample"}, &makeAlpha});
    passed &= require(factoryCalls == 0, "registration invoked its factory");

    const std::vector<engine::sample::SampleDescriptor> descriptors = registry.list();
    passed &= require(factoryCalls == 0, "listing invoked a factory");
    passed &= require(
        descriptors.size() == 1 && descriptors.front().name == "alpha",
        "listing did not preserve registration metadata");

    std::unique_ptr<engine::sample::Sample> sample = registry.create("alpha");
    passed &= require(factoryCalls == 1 && sample != nullptr, "create did not invoke its factory");
    passed &= require(registry.create("missing") == nullptr, "unknown sample did not return null");

    passed &= require(
        throwsLogicError([] {
            engine::sample::SampleRegistry invalid;
            invalid.add({{"invalid-", "Summary"}, &makeAlpha});
        }),
        "trailing-hyphen name was accepted");
    passed &= require(
        throwsLogicError([] {
            engine::sample::SampleRegistry invalid;
            invalid.add({{"valid", "two\nlines"}, &makeAlpha});
        }),
        "multiline summary was accepted");
    passed &= require(
        throwsLogicError([] {
            engine::sample::SampleRegistry invalid;
            invalid.add({{"valid", "Summary"}, nullptr});
        }),
        "null factory was accepted");
    passed &= require(
        throwsLogicError([] {
            engine::sample::SampleRegistry duplicate;
            duplicate.add({{"alpha", "First"}, &makeAlpha});
            duplicate.add({{"alpha", "Second"}, &makeAlpha});
        }),
        "duplicate name was accepted");
    passed &= require(
        throwsLogicError([] {
            engine::sample::SampleRegistry invalid;
            invalid.add({{"null", "Null sample"}, &makeNull});
            static_cast<void>(invalid.create("null"));
        }),
        "null factory result was accepted");
    passed &= require(
        throwsLogicError([] {
            engine::sample::SampleRegistry invalid;
            invalid.add({{"mismatch", "Mismatch sample"}, &makeMismatch});
            static_cast<void>(invalid.create("mismatch"));
        }),
        "factory descriptor mismatch was accepted");

    return passed ? 0 : 1;
}
