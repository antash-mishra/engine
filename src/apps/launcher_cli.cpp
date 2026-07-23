#include <engine/apps/launcher_cli.h>

#include <charconv>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace engine::apps {
namespace {

constexpr unsigned int maxDimension = 16384;
constexpr unsigned int maxFrameLimit = 10000000;

unsigned int parsePositiveUnsigned(
    const std::string& option,
    const char* value,
    unsigned int maximum) {
    if (value == nullptr || *value == '\0') {
        throw std::invalid_argument(option + " requires a value");
    }

    const std::string_view text(value);
    unsigned int parsed = 0;
    const auto result =
        std::from_chars(text.data(), text.data() + text.size(), parsed, 10);
    if (result.ec != std::errc() || result.ptr != text.data() + text.size() ||
        parsed == 0 || parsed > maximum) {
        throw std::invalid_argument(
            option + " requires an integer from 1 to " + std::to_string(maximum));
    }
    return parsed;
}

const char* requireValue(int& index, int argc, char** argv, const std::string& option) {
    if (index + 1 >= argc || argv[index + 1] == nullptr ||
        std::string_view(argv[index + 1]).rfind("--", 0) == 0) {
        throw std::invalid_argument(option + " requires a value");
    }
    ++index;
    return argv[index];
}

void markUnique(std::unordered_set<std::string>& seen, const std::string& option) {
    if (!seen.insert(option).second) {
        throw std::invalid_argument("duplicate option: " + option);
    }
}

}  // namespace

LauncherOptions parseLauncherOptions(int argc, char** argv) {
    LauncherOptions options;
    std::unordered_set<std::string> seen;
    for (int index = 1; index < argc; ++index) {
        const std::string option = argv[index] == nullptr ? "" : argv[index];
        if (option == "--help") {
            markUnique(seen, option);
            options.showHelp = true;
        } else if (option == "--list-samples") {
            markUnique(seen, option);
            options.listSamples = true;
        } else if (option == "--sample") {
            markUnique(seen, option);
            options.sampleName = requireValue(index, argc, argv, option);
        } else if (option == "--frames") {
            markUnique(seen, option);
            options.frameLimit = parsePositiveUnsigned(
                option, requireValue(index, argc, argv, option), maxFrameLimit);
        } else if (option == "--width") {
            markUnique(seen, option);
            options.width = static_cast<int>(parsePositiveUnsigned(
                option, requireValue(index, argc, argv, option), maxDimension));
        } else if (option == "--height") {
            markUnique(seen, option);
            options.height = static_cast<int>(parsePositiveUnsigned(
                option, requireValue(index, argc, argv, option), maxDimension));
        } else if (option == "--asset-root") {
            markUnique(seen, option);
            options.assetRoot = requireValue(index, argc, argv, option);
        } else {
            throw std::invalid_argument("unknown argument: " + option);
        }
    }

    const bool hasRunArguments =
        options.sampleName.has_value() || options.frameLimit.has_value() ||
        seen.count("--width") != 0 || seen.count("--height") != 0 ||
        options.assetRoot.has_value();
    if (options.showHelp && (options.listSamples || hasRunArguments)) {
        throw std::invalid_argument("--help cannot be combined with other options");
    }
    if (options.listSamples && hasRunArguments) {
        throw std::invalid_argument("--list-samples cannot be combined with run options");
    }
    if (!options.showHelp && !options.listSamples && !options.sampleName.has_value()) {
        throw std::invalid_argument(
            "missing required --sample argument (or use --list-samples)");
    }
    return options;
}

std::string launcherUsage() {
    return
        "Usage:\n"
        "  render_samples --list-samples\n"
        "  render_samples --sample NAME [--frames COUNT] [--width PIXELS]\n"
        "                 [--height PIXELS] [--asset-root PATH]\n"
        "  render_samples --help\n";
}

}  // namespace engine::apps
