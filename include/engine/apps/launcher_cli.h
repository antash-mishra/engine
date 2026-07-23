#ifndef ENGINE_APPS_LAUNCHER_CLI_H
#define ENGINE_APPS_LAUNCHER_CLI_H

#include <filesystem>
#include <optional>
#include <string>

namespace engine::apps {

struct LauncherOptions {
    bool showHelp = false;
    bool listSamples = false;
    std::optional<std::string> sampleName;
    std::optional<unsigned int> frameLimit;
    int width = 800;
    int height = 600;
    std::optional<std::filesystem::path> assetRoot;
};

// Strictly parses the complete argument list. Unknown, duplicated, conflicting,
// missing-value, signed, zero, overflowed, and prefix-only numeric arguments
// throw std::invalid_argument; no partial parse is returned.
LauncherOptions parseLauncherOptions(int argc, char** argv);
std::string launcherUsage();

}  // namespace engine::apps

#endif
