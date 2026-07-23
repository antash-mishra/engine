#ifndef ENGINE_ASSETS_ASSET_ROOT_H
#define ENGINE_ASSETS_ASSET_ROOT_H

#include <filesystem>
#include <optional>
#include <string>

namespace engine::assets {

enum class AssetRootSource {
    CommandLine,
    Environment,
    ExecutableRelative,
};

// A validated, immutable base directory for runtime asset lookup.
//
// Resolution precedence is command line, ENGINE_ASSET_ROOT, then an "assets"
// directory beside the executable. An explicitly selected root that is absent
// is an error and never falls through. resolve() and file() throw
// std::runtime_error on invalid or escaping paths. AssetRoot does not own file
// contents and is safe to share between samples after construction.
class AssetRoot final {
public:
    static AssetRoot resolve(
        const std::optional<std::filesystem::path>& commandLineRoot,
        const std::filesystem::path& executable);

    const std::filesystem::path& path() const noexcept;
    AssetRootSource source() const noexcept;
    std::filesystem::path file(const std::filesystem::path& relativePath) const;

private:
    AssetRoot(std::filesystem::path root, AssetRootSource source);

    std::filesystem::path root_;
    AssetRootSource source_;
};

std::string toString(AssetRootSource source);

}  // namespace engine::assets

#endif
