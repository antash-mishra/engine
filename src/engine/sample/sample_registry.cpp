#include <engine/sample/sample_registry.h>

#include <algorithm>
#include <stdexcept>

namespace engine::sample {
namespace {

bool validName(const std::string& name) {
    if (name.empty() || name.front() == '-' || name.back() == '-') {
        return false;
    }
    return std::all_of(
        name.begin(),
        name.end(),
        [](char character) {
            return (character >= 'a' && character <= 'z') ||
                (character >= '0' && character <= '9') || character == '-';
        });
}

bool validSummary(const std::string& summary) {
    return !summary.empty() &&
        summary.find_first_of("\r\n\t") == std::string::npos;
}

}  // namespace

SampleRegistration rayMarchingSampleRegistration();

void SampleRegistry::add(SampleRegistration registration) {
    if (registration.factory == nullptr) {
        throw std::logic_error("cannot register a null sample factory");
    }
    if (!validName(registration.descriptor.name)) {
        throw std::logic_error(
            "sample name must contain lowercase ASCII letters, digits, or internal hyphens");
    }
    if (!validSummary(registration.descriptor.summary)) {
        throw std::logic_error("sample summary must be a non-empty single line");
    }
    const auto duplicate = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&registration](const Entry& entry) {
            return entry.descriptor.name == registration.descriptor.name;
        });
    if (duplicate != entries_.end()) {
        throw std::logic_error(
            "duplicate sample name: " + registration.descriptor.name);
    }
    entries_.push_back({
        std::move(registration.descriptor),
        registration.factory,
    });
}

std::vector<SampleDescriptor> SampleRegistry::list() const {
    std::vector<SampleDescriptor> descriptors;
    descriptors.reserve(entries_.size());
    for (const Entry& entry : entries_) {
        descriptors.push_back(entry.descriptor);
    }
    std::sort(
        descriptors.begin(),
        descriptors.end(),
        [](const SampleDescriptor& left, const SampleDescriptor& right) {
            return left.name < right.name;
        });
    return descriptors;
}

std::unique_ptr<Sample> SampleRegistry::create(const std::string& name) const {
    const auto match = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&name](const Entry& entry) {
            return entry.descriptor.name == name;
        });
    if (match == entries_.end()) {
        return nullptr;
    }
    std::unique_ptr<Sample> sample = match->factory();
    if (!sample) {
        throw std::logic_error("sample factory returned null: " + name);
    }
    if (sample->descriptor().name != match->descriptor.name) {
        throw std::logic_error("sample factory descriptor mismatch: " + name);
    }
    return sample;
}

SampleRegistry createDefaultRegistry() {
    SampleRegistry registry;
    registry.add(rayMarchingSampleRegistration());
    return registry;
}

}  // namespace engine::sample
