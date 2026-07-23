#include <engine/apps/launcher_cli.h>
#include <engine/assets/asset_root.h>
#include <engine/platform/window.h>
#include <engine/sample/sample_registry.h>

#include <exception>
#include <iostream>

namespace {

int run(int argc, char** argv) {
    engine::apps::LauncherOptions options;
    try {
        options = engine::apps::parseLauncherOptions(argc, argv);
    } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << '\n'
                  << engine::apps::launcherUsage();
        return 2;
    }

    if (options.showHelp) {
        std::cout << engine::apps::launcherUsage();
        return 0;
    }

    const engine::sample::SampleRegistry registry =
        engine::sample::createDefaultRegistry();
    if (options.listSamples) {
        for (const auto& descriptor : registry.list()) {
            std::cout << descriptor.name << '\t' << descriptor.summary << '\n';
        }
        return 0;
    }

    std::unique_ptr<engine::sample::Sample> sample =
        registry.create(*options.sampleName);
    if (!sample) {
        std::cerr << "error: unknown sample: " << *options.sampleName << '\n';
        return 2;
    }

    engine::assets::AssetRoot assetRoot =
        engine::assets::AssetRoot::resolve(
            options.assetRoot,
            engine::platform::executablePath(argc > 0 ? argv[0] : nullptr));
    const engine::sample::SampleRunResult result = sample->run({
        options.width,
        options.height,
        options.frameLimit,
        std::move(assetRoot),
    });

    // This single machine-readable record is the fixed-frame acceptance
    // evidence. Tests reject missing, duplicate, or mismatched completion data.
    std::cout << "{\"event\":\"sample_complete\",\"sample\":\""
              << sample->descriptor().name << "\",\"frames_rendered\":"
              << result.framesRendered << "}\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
