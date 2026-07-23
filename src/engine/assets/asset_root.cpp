#include <engine/assets/asset_root.h>

#include <cstdlib>
#include <stdexcept>

namespace engine::assets {
namespace {

std::filesystem::path validateRoot(
    const std::filesystem::path& candidate,
    const std::string& origin) {
    if (candidate.empty()) {
        throw std::runtime_error("asset root from " + origin + " is empty");
    }

    std::error_code error;
    const auto canonical = std::filesystem::canonical(candidate, error);
    if (error || !std::filesystem::is_directory(canonical)) {
        throw std::runtime_error(
            "asset root from " + origin + " does not exist or is not a directory: " +
            candidate.string());
    }
    return canonical;
}

bool isWithin(
    const std::filesystem::path& parent,
    const std::filesystem::path& child) {
    auto parentPart = parent.begin();
    auto childPart = child.begin();
    for (; parentPart != parent.end(); ++parentPart, ++childPart) {
        if (childPart == child.end() || *parentPart != *childPart) {
            return false;
        }
    }
    return true;
}

}  // namespace

AssetRoot::AssetRoot(std::filesystem::path root, AssetRootSource source)
    : root_(std::move(root)), source_(source) {}

AssetRoot AssetRoot::resolve(
    const std::optional<std::filesystem::path>& commandLineRoot,
    const std::filesystem::path& executable) {
    if (commandLineRoot.has_value()) {
        return AssetRoot(
            validateRoot(*commandLineRoot, "--asset-root"),
            AssetRootSource::CommandLine);
    }

    if (const char* environmentRoot = std::getenv("ENGINE_ASSET_ROOT");
        environmentRoot != nullptr) {
        return AssetRoot(
            validateRoot(environmentRoot, "ENGINE_ASSET_ROOT"),
            AssetRootSource::Environment);
    }

    return AssetRoot(
        validateRoot(executable.parent_path() / "assets", "executable-relative default"),
        AssetRootSource::ExecutableRelative);
}

const std::filesystem::path& AssetRoot::path() const noexcept {
    return root_;
}

AssetRootSource AssetRoot::source() const noexcept {
    return source_;
}

std::filesystem::path AssetRoot::file(
    const std::filesystem::path& relativePath) const {
    if (relativePath.empty() || relativePath.is_absolute()) {
        throw std::runtime_error("asset path must be a non-empty relative path");
    }

    std::error_code error;
    const auto canonical = std::filesystem::canonical(root_ / relativePath, error);
    if (error || !std::filesystem::is_regular_file(canonical)) {
        throw std::runtime_error("required asset does not exist: " + relativePath.string());
    }
    if (!isWithin(root_, canonical)) {
        throw std::runtime_error("asset path escapes the selected asset root");
    }
    return canonical;
}

std::string toString(AssetRootSource source) {
    switch (source) {
        case AssetRootSource::CommandLine:
            return "command-line";
        case AssetRootSource::Environment:
            return "environment";
        case AssetRootSource::ExecutableRelative:
            return "executable-relative";
    }
    throw std::logic_error("unknown AssetRootSource");
}

}  // namespace engine::assets
