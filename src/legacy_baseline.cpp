#include "legacy_baseline.h"

#include <json/json.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace legacy_baseline {
namespace {

using Json = nlohmann::json;

double milliseconds(TimePoint begin, TimePoint end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

std::array<float, 3> readVec3(const Json& value, const char* field) {
    const auto& vector = value.at(field);
    if (!vector.is_array() || vector.size() != 3) {
        throw std::runtime_error(std::string("baseline camera.") + field + " must contain 3 values");
    }
    return {vector.at(0).get<float>(), vector.at(1).get<float>(), vector.at(2).get<float>()};
}

Config loadConfig(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("cannot open legacy baseline config: " + path.string());
    }

    Json json;
    stream >> json;
    Config result;
    result.enabled = true;
    result.width = json.at("width").get<unsigned int>();
    result.height = json.at("height").get<unsigned int>();
    result.warmupFrames = json.at("warmup_frames").get<unsigned int>();
    result.measuredFrames = json.at("measured_frames").get<unsigned int>();
    result.seed = json.at("seed").get<std::uint32_t>();
    result.fixedTimestepSeconds = json.at("fixed_timestep_seconds").get<double>();
    result.resourceRoot = json.at("resource_root").get<std::string>();
    result.artifactRoot = json.at("artifact_root").get<std::string>();
    result.childResultPath = json.at("child_result").get<std::string>();

    const auto& camera = json.at("camera");
    result.camera.position = readVec3(camera, "position");
    result.camera.forward = readVec3(camera, "forward");
    result.camera.up = readVec3(camera, "up");
    result.camera.verticalFovDegrees = camera.at("vertical_fov_degrees").get<float>();
    result.camera.nearPlane = camera.at("near_plane").get<float>();
    result.camera.farPlane = camera.at("far_plane").get<float>();

    if (result.width == 0 || result.height == 0 || result.measuredFrames == 0 ||
        result.fixedTimestepSeconds <= 0.0) {
        throw std::runtime_error("legacy baseline dimensions, measured frames, and timestep must be positive");
    }
    return result;
}

double percentile95(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const std::size_t rank =
        static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(values.size())));
    return values[std::max<std::size_t>(1, rank) - 1];
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    if (values.size() % 2 == 0) {
        return (values[middle - 1] + values[middle]) * 0.5;
    }
    return values[middle];
}

std::string enumHex(GLenum value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << value;
    return stream.str();
}

}  // namespace

Session::Session(int argc, char** argv, std::string sample, TimePoint processStart)
    : sample_(std::move(sample)), processStart_(processStart) {
    if (argc == 1) {
        return;
    }
    if (argc != 3 || std::string(argv[1]) != "--legacy-baseline-config") {
        throw std::runtime_error(
            "usage: executable [--legacy-baseline-config <config.json>]");
    }
    config_ = loadConfig(argv[2]);
    std::filesystem::create_directories(config_.artifactRoot);
    if (!config_.childResultPath.parent_path().empty()) {
        std::filesystem::create_directories(config_.childResultPath.parent_path());
    }
}

double Session::fixedTimeSeconds() const {
    return static_cast<double>(frameIndex_) * config_.fixedTimestepSeconds;
}

void Session::addStartupSpan(
    const std::string& name,
    TimePoint begin,
    TimePoint end,
    const std::string& thread) {
    if (enabled()) {
        spans_.push_back({name, milliseconds(begin, end), thread});
    }
}

void Session::installGlDiagnostics() {
    if (!enabled()) {
        return;
    }
    while (glGetError() != GL_NO_ERROR) {
    }
    if (glDebugMessageCallback != nullptr) {
        debugOutputAvailable_ = true;
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(&Session::debugCallback, this);
        glDebugMessageControl(
            GL_DONT_CARE,
            GL_DONT_CARE,
            GL_DEBUG_SEVERITY_NOTIFICATION,
            0,
            nullptr,
            GL_FALSE);
    }
    gpuTimerAvailable_ =
        GLAD_GL_VERSION_3_3 != 0 &&
        glGenQueries != nullptr &&
        glBeginQuery != nullptr &&
        glEndQuery != nullptr &&
        glGetQueryObjectui64v != nullptr;
    if (gpuTimerAvailable_) {
        gpuQueries_.resize(config_.measuredFrames);
        glGenQueries(
            static_cast<GLsizei>(gpuQueries_.size()),
            gpuQueries_.data());
    }
}

TimePoint Session::beginFrame() {
    if (enabled() && gpuTimerAvailable_ &&
        frameIndex_ >= config_.warmupFrames &&
        frameIndex_ < config_.warmupFrames + config_.measuredFrames) {
        const std::size_t measuredIndex =
            static_cast<std::size_t>(frameIndex_ - config_.warmupFrames);
        glBeginQuery(GL_TIME_ELAPSED, gpuQueries_[measuredIndex]);
        gpuQueryActive_ = true;
    }
    return Clock::now();
}

void Session::endCpuFrame(TimePoint begin) {
    if (!enabled()) {
        return;
    }
    if (frameIndex_ >= config_.warmupFrames &&
        frameIndex_ < config_.warmupFrames + config_.measuredFrames) {
        cpuFrameMs_.push_back(milliseconds(begin, Clock::now()));
    }
    if (gpuQueryActive_) {
        glEndQuery(GL_TIME_ELAPSED);
        gpuQueryActive_ = false;
    }
}

void Session::markFirstFrameGpuComplete() {
    if (!enabled() || firstFrameRecorded_) {
        return;
    }
    glFinish();
    firstFrameMs_ = milliseconds(processStart_, Clock::now());
    firstFrameRecorded_ = true;
}

bool Session::isFinalCaptureFrame() const {
    return enabled() &&
        frameIndex_ + 1 == config_.warmupFrames + config_.measuredFrames;
}

void Session::captureDefaultColor(const std::string& name, int width, int height) {
    if (!enabled()) {
        return;
    }
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    GLint oldPackAlignment = 0;
    GLint oldReadBuffer = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &oldPackAlignment);
    glGetIntegerv(GL_READ_BUFFER, &oldReadBuffer);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glReadBuffer(static_cast<GLenum>(oldReadBuffer));
    glPixelStorei(GL_PACK_ALIGNMENT, oldPackAlignment);
    writeBytes(
        name,
        "default_back_buffer",
        width,
        height,
        4,
        "uint8",
        "display_encoded",
        pixels.data(),
        pixels.size());
}

void Session::captureTextureFloat(
    const std::string& name,
    GLuint texture,
    int width,
    int height,
    GLenum format,
    int channels,
    const std::string& dataSpace) {
    if (!enabled()) {
        return;
    }
    std::vector<float> pixels(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
        static_cast<std::size_t>(channels));
    GLint oldTexture = 0;
    GLint oldPackAlignment = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
    glGetIntegerv(GL_PACK_ALIGNMENT, &oldPackAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexImage(GL_TEXTURE_2D, 0, format, GL_FLOAT, pixels.data());
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture));
    glPixelStorei(GL_PACK_ALIGNMENT, oldPackAlignment);
    writeBytes(
        name,
        "texture_2d",
        width,
        height,
        channels,
        "float32",
        dataSpace,
        pixels.data(),
        pixels.size() * sizeof(float));
}

bool Session::advanceFrame() {
    if (!enabled()) {
        return false;
    }
    ++frameIndex_;
    return frameIndex_ == config_.warmupFrames + config_.measuredFrames;
}

int Session::finish(int requestedExitCode) {
    if (!enabled()) {
        return requestedExitCode;
    }

    if (gpuTimerAvailable_) {
        gpuFrameMs_.reserve(gpuQueries_.size());
        for (GLuint query : gpuQueries_) {
            GLuint64 nanoseconds = 0;
            glGetQueryObjectui64v(query, GL_QUERY_RESULT, &nanoseconds);
            gpuFrameMs_.push_back(static_cast<double>(nanoseconds) / 1'000'000.0);
        }
        glDeleteQueries(
            static_cast<GLsizei>(gpuQueries_.size()),
            gpuQueries_.data());
        gpuQueries_.clear();
    }
    if (!debugOutputAvailable_) {
        for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError()) {
            const std::string message = "glGetError: " + enumHex(error);
            recordDebugMessage(
                GL_DEBUG_SOURCE_API,
                GL_DEBUG_TYPE_ERROR,
                error,
                GL_DEBUG_SEVERITY_HIGH,
                message.c_str(),
                static_cast<GLsizei>(message.size()));
        }
    }

    int exitCode = requestedExitCode;
    if (glErrorCount_ != 0 || nonFiniteCount_ != 0 || !firstFrameRecorded_ ||
        cpuFrameMs_.size() != config_.measuredFrames ||
        !gpuTimerAvailable_ || gpuFrameMs_.size() != config_.measuredFrames ||
        readbacks_.empty()) {
        exitCode = exitCode == 0 ? 1 : exitCode;
    }

    Json json;
    json["sample"] = sample_;
    json["gpu"] = {
        {"name", reinterpret_cast<const char*>(glGetString(GL_RENDERER))},
        {"driver", reinterpret_cast<const char*>(glGetString(GL_VERSION))},
        {"api", reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))},
        {"vendor", reinterpret_cast<const char*>(glGetString(GL_VENDOR))},
    };
    json["startup"]["total_to_first_frame_ms"] = firstFrameMs_;
    json["startup"]["boundary"] = "GPU complete after first final draw, before readback and swap";
    json["startup"]["spans"] = Json::array();
    for (const Span& span : spans_) {
        json["startup"]["spans"].push_back({
            {"name", span.name},
            {"duration_ms", span.durationMs},
            {"thread", span.thread},
        });
    }
    json["frames"] = {
        {"raw_cpu_ms", cpuFrameMs_},
        {"raw_gpu_ms", gpuFrameMs_},
        {"cpu_median_ms", cpuFrameMs_.empty() ? 0.0 : median(cpuFrameMs_)},
        {"cpu_p95_ms", cpuFrameMs_.empty() ? 0.0 : percentile95(cpuFrameMs_)},
        {"gpu_median_ms", gpuFrameMs_.empty() ? 0.0 : median(gpuFrameMs_)},
        {"gpu_p95_ms", gpuFrameMs_.empty() ? 0.0 : percentile95(gpuFrameMs_)},
    };
    json["memory"] = {
        {"peak_rss_bytes", peakRssBytes()},
        {"retained_cpu_bytes_after_first_frame", retainedCpuBytes_},
        {"retained_cpu_scope", retainedCpuScope_},
        {"metric", "Linux /proc/self/status VmHWM through final capture"},
    };
    json["readbacks"] = Json::array();
    for (const Readback& readback : readbacks_) {
        Json stats = Json::array();
        for (const ChannelStats& channel : readback.channelStats) {
            stats.push_back({
                {"min", channel.minimum},
                {"max", channel.maximum},
                {"mean", channel.mean},
            });
        }
        json["readbacks"].push_back({
            {"name", readback.name},
            {"source", readback.source},
            {"frame_index", readback.frameIndex},
            {"width", readback.width},
            {"height", readback.height},
            {"channels", readback.channels},
            {"component_type", readback.componentType},
            {"byte_order", "little"},
            {"row_order", "bottom_to_top"},
            {"data_space", readback.dataSpace},
            {"raw_path", readback.path.string()},
            {"byte_count", readback.byteCount},
            {"finite_count", readback.finiteCount},
            {"non_finite_count", readback.nonFiniteCount},
            {"channel_statistics", stats},
        });
    }
    json["validation"] = {
        {"gl_debug_available", debugOutputAvailable_},
        {"gl_debug_message_count", glMessageCount_},
        {"gl_debug_warning_count", glWarningCount_},
        {"gl_debug_error_count", glErrorCount_},
        {"non_finite_value_count", nonFiniteCount_},
        {"exit_code", exitCode},
    };
    if (!gpuTimerAvailable_) {
        json["validation"]["notes"] = Json::array(
            {"GL_TIME_ELAPSED query support is unavailable"});
    }

    const std::filesystem::path debugPath = config_.artifactRoot / "gl-debug.jsonl";
    std::ofstream debugStream(debugPath);
    for (const DebugMessage& message : debugMessages_) {
        debugStream << Json({
            {"source", message.source},
            {"type", message.type},
            {"id", message.id},
            {"severity", message.severity},
            {"message", message.message},
        }).dump() << '\n';
    }
    debugStream.close();
    if (!debugStream) {
        return 2;
    }
    json["validation"]["gl_debug_path"] = debugPath.string();

    std::ofstream output(config_.childResultPath);
    output << std::setw(2) << json << '\n';
    if (!output) {
        return 2;
    }
    return exitCode;
}

void Session::setRetainedCpuBytes(std::uint64_t bytes, std::string scope) {
    if (!enabled()) {
        return;
    }
    retainedCpuBytes_ = bytes;
    retainedCpuScope_ = std::move(scope);
}

void APIENTRY Session::debugCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam) {
    auto* session = const_cast<Session*>(static_cast<const Session*>(userParam));
    session->recordDebugMessage(source, type, id, severity, message, length);
}

void Session::recordDebugMessage(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    const char* message,
    GLsizei length) {
    debugMessages_.push_back({
        source,
        type,
        id,
        severity,
        std::string(message, message + std::max<GLsizei>(0, length)),
    });
    ++glMessageCount_;
    if (type == GL_DEBUG_TYPE_ERROR || severity == GL_DEBUG_SEVERITY_HIGH) {
        ++glErrorCount_;
    } else if (type == GL_DEBUG_TYPE_PERFORMANCE ||
               severity == GL_DEBUG_SEVERITY_MEDIUM) {
        ++glWarningCount_;
    }
}

void Session::writeBytes(
    const std::string& name,
    const std::string& source,
    int width,
    int height,
    int channels,
    const std::string& componentType,
    const std::string& dataSpace,
    const void* bytes,
    std::size_t byteCount) {
    const std::string extension = componentType == "float32" ? ".f32.bin" : ".u8.bin";
    const std::filesystem::path path = config_.artifactRoot / (name + extension);
    std::ofstream output(path, std::ios::binary);
    output.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(byteCount));
    if (!output) {
        throw std::runtime_error("cannot write readback: " + path.string());
    }

    Readback readback;
    readback.name = name;
    readback.source = source;
    readback.path = path;
    readback.frameIndex = frameIndex_;
    readback.width = width;
    readback.height = height;
    readback.channels = channels;
    readback.componentType = componentType;
    readback.dataSpace = dataSpace;
    readback.byteCount = byteCount;
    readback.channelStats.resize(static_cast<std::size_t>(channels));

    const std::size_t valueCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
        static_cast<std::size_t>(channels);
    std::vector<long double> sums(static_cast<std::size_t>(channels), 0.0);
    std::vector<std::uint64_t> counts(static_cast<std::size_t>(channels), 0);
    for (ChannelStats& stats : readback.channelStats) {
        stats.minimum = std::numeric_limits<double>::infinity();
        stats.maximum = -std::numeric_limits<double>::infinity();
    }

    for (std::size_t index = 0; index < valueCount; ++index) {
        const double value = componentType == "float32"
            ? static_cast<const float*>(bytes)[index]
            : static_cast<const std::uint8_t*>(bytes)[index];
        if (!std::isfinite(value)) {
            ++readback.nonFiniteCount;
            ++nonFiniteCount_;
            continue;
        }
        ++readback.finiteCount;
        const std::size_t channel = index % static_cast<std::size_t>(channels);
        ChannelStats& stats = readback.channelStats[channel];
        stats.minimum = std::min(stats.minimum, value);
        stats.maximum = std::max(stats.maximum, value);
        sums[channel] += value;
        ++counts[channel];
    }
    for (std::size_t channel = 0; channel < readback.channelStats.size(); ++channel) {
        ChannelStats& stats = readback.channelStats[channel];
        if (counts[channel] == 0) {
            stats.minimum = 0.0;
            stats.maximum = 0.0;
            stats.mean = 0.0;
        } else {
            stats.mean = static_cast<double>(sums[channel] / counts[channel]);
        }
    }
    readbacks_.push_back(std::move(readback));
}

std::uint64_t Session::peakRssBytes() const {
    std::ifstream status("/proc/self/status");
    std::string key;
    while (status >> key) {
        if (key == "VmHWM:") {
            std::uint64_t kibibytes = 0;
            std::string unit;
            status >> kibibytes >> unit;
            return kibibytes * 1024;
        }
        status.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    return 0;
}

}  // namespace legacy_baseline
