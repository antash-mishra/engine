#ifndef LEGACY_BASELINE_H
#define LEGACY_BASELINE_H

#include <glad/glad.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace legacy_baseline {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

struct CameraSettings {
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
    std::array<float, 3> forward{0.0f, 0.0f, -1.0f};
    std::array<float, 3> up{0.0f, 1.0f, 0.0f};
    float verticalFovDegrees = 35.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};

struct Config {
    bool enabled = false;
    unsigned int width = 800;
    unsigned int height = 600;
    unsigned int warmupFrames = 0;
    unsigned int measuredFrames = 1;
    std::uint32_t seed = 1;
    double fixedTimestepSeconds = 1.0 / 60.0;
    std::filesystem::path resourceRoot;
    std::filesystem::path artifactRoot;
    std::filesystem::path childResultPath;
    CameraSettings camera;
};

// Phase 0 instrumentation for the two legacy executables. No argument means
// inactive, so the existing interactive program remains the default behavior.
class Session {
public:
    Session(
        int argc,
        char** argv,
        std::string sample,
        TimePoint processStart);

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool enabled() const { return config_.enabled; }
    const Config& config() const { return config_; }
    std::uint64_t frameIndex() const { return frameIndex_; }
    double fixedTimeSeconds() const;

    void addStartupSpan(
        const std::string& name,
        TimePoint begin,
        TimePoint end,
        const std::string& thread = "main");

    // Call after GLAD is loaded. Debug messages are synchronous so a reported
    // frame can be associated with every API error observed during capture.
    void installGlDiagnostics();

    TimePoint beginFrame();
    void endCpuFrame(TimePoint begin);
    void markFirstFrameGpuComplete();
    bool isFinalCaptureFrame() const;

    void captureDefaultColor(const std::string& name, int width, int height);
    void captureTextureFloat(
        const std::string& name,
        GLuint texture,
        int width,
        int height,
        GLenum format,
        int channels,
        const std::string& dataSpace);

    // Advances the fixed-frame run and returns true after exactly the configured
    // warmup and measured frames have completed.
    bool advanceFrame();

    // Records a defensible lower bound, not allocator-wide retained memory.
    // The scope string must state exactly which still-live containers are counted.
    void setRetainedCpuBytes(std::uint64_t bytes, std::string scope);

    // Writes the child result while the GL context is still alive. A nonzero
    // return means API errors, non-finite float data, or an earlier failure.
    int finish(int requestedExitCode = 0);

private:
    struct Span {
        std::string name;
        double durationMs = 0.0;
        std::string thread;
    };

    struct ChannelStats {
        double minimum = 0.0;
        double maximum = 0.0;
        double mean = 0.0;
    };

    struct Readback {
        std::string name;
        std::string source;
        std::filesystem::path path;
        std::uint64_t frameIndex = 0;
        int width = 0;
        int height = 0;
        int channels = 0;
        std::string componentType;
        std::string dataSpace;
        std::uint64_t byteCount = 0;
        std::uint64_t finiteCount = 0;
        std::uint64_t nonFiniteCount = 0;
        std::vector<ChannelStats> channelStats;
    };

    struct DebugMessage {
        GLenum source = 0;
        GLenum type = 0;
        GLuint id = 0;
        GLenum severity = 0;
        std::string message;
    };

    static void APIENTRY debugCallback(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* userParam);

    void recordDebugMessage(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        const char* message,
        GLsizei length);
    void writeBytes(
        const std::string& name,
        const std::string& source,
        int width,
        int height,
        int channels,
        const std::string& componentType,
        const std::string& dataSpace,
        const void* bytes,
        std::size_t byteCount);
    std::uint64_t peakRssBytes() const;

    Config config_;
    std::string sample_;
    TimePoint processStart_;
    std::uint64_t frameIndex_ = 0;
    double firstFrameMs_ = 0.0;
    bool firstFrameRecorded_ = false;
    bool debugOutputAvailable_ = false;
    bool gpuTimerAvailable_ = false;
    bool gpuQueryActive_ = false;
    std::uint64_t glMessageCount_ = 0;
    std::uint64_t glWarningCount_ = 0;
    std::uint64_t glErrorCount_ = 0;
    std::uint64_t nonFiniteCount_ = 0;
    std::vector<Span> spans_;
    std::vector<double> cpuFrameMs_;
    std::vector<double> gpuFrameMs_;
    std::vector<GLuint> gpuQueries_;
    std::vector<Readback> readbacks_;
    std::vector<DebugMessage> debugMessages_;
    std::uint64_t retainedCpuBytes_ = 0;
    std::string retainedCpuScope_ = "not reported";
};

}  // namespace legacy_baseline

#endif
