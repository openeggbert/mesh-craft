#pragma once

#include <vector>

namespace MeshCraft::Editor {

// Owns only the lifecycle of MeshCraft's headless --benchmark frame sample:
// whether it is enabled, how many real frames still need timing, the captured
// durations, completion hand-off, and the LoadContent duration. Rendering and
// benchmark-category work deliberately remain with MeshCraftApplication.
class BenchmarkProgress {
public:
    static constexpr int kDefaultFrameCount = 10;

    explicit BenchmarkProgress(bool enabled = false, int frameCount = kDefaultFrameCount);

    [[nodiscard]] bool enabled() const { return enabled_; }
    [[nodiscard]] bool recordingFrames() const { return enabled_ && framesRemaining_ > 0; }
    [[nodiscard]] int framesRemaining() const { return framesRemaining_; }
    [[nodiscard]] const std::vector<double>& frameTimesMs() const { return frameTimesMs_; }
    [[nodiscard]] double loadContentMs() const { return loadContentMs_; }

    void setLoadContentMs(double milliseconds);
    void recordFrame(double milliseconds);

    // Completion is consumed once by EndDraw(), which runs the rest of the
    // benchmark suite and exits. Calling again reports false.
    [[nodiscard]] bool consumeCompletion();

private:
    bool enabled_{false};
    int framesRemaining_{0};
    bool completionPending_{false};
    double loadContentMs_{0.0};
    std::vector<double> frameTimesMs_;
};

} // namespace MeshCraft::Editor
