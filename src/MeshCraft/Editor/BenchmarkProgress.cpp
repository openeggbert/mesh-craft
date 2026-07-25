#include "MeshCraft/Editor/BenchmarkProgress.hpp"

#include <algorithm>

namespace MeshCraft::Editor {

BenchmarkProgress::BenchmarkProgress(bool enabled, int frameCount)
    : enabled_(enabled)
    , framesRemaining_(enabled ? std::max(1, frameCount) : 0)
{
    if (enabled_) frameTimesMs_.reserve(static_cast<std::size_t>(framesRemaining_));
}

void BenchmarkProgress::setLoadContentMs(double milliseconds) {
    loadContentMs_ = milliseconds;
}

void BenchmarkProgress::recordFrame(double milliseconds) {
    if (!recordingFrames()) return;
    frameTimesMs_.push_back(milliseconds);
    if (--framesRemaining_ == 0)
        completionPending_ = true;
}

bool BenchmarkProgress::consumeCompletion() {
    if (!completionPending_) return false;
    completionPending_ = false;
    return true;
}

} // namespace MeshCraft::Editor
