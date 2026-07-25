#include "MeshCraft/Editor/BenchmarkProgress.hpp"

#include <cmath>
#include <iostream>

using MeshCraft::Editor::BenchmarkProgress;

static int failures = 0;
static void check(bool condition, const char* message) {
    if (condition) std::cout << "PASS: " << message << '\n';
    else { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

int main() {
    {
        BenchmarkProgress inactive;
        inactive.recordFrame(1.0);
        check(!inactive.enabled(), "default progress is disabled");
        check(!inactive.recordingFrames(), "disabled progress records no frames");
        check(inactive.frameTimesMs().empty(), "disabled progress ignores frame samples");
        check(!inactive.consumeCompletion(), "disabled progress never completes");
    }
    {
        BenchmarkProgress progress(true, 2);
        check(progress.enabled(), "enabled progress retains benchmark mode");
        check(progress.recordingFrames(), "enabled progress initially records frames");
        check(progress.framesRemaining() == 2, "configured frame target is retained");

        progress.setLoadContentMs(12.5);
        check(std::abs(progress.loadContentMs() - 12.5) < 1e-9, "startup duration round-trips");

        progress.recordFrame(3.0);
        check(progress.framesRemaining() == 1, "one frame decrements the remaining count");
        check(!progress.consumeCompletion(), "completion waits for every requested frame");

        progress.recordFrame(4.0);
        check(!progress.recordingFrames(), "final sample stops frame recording");
        check(progress.frameTimesMs().size() == 2, "all requested frame samples are retained");
        check(std::abs(progress.frameTimesMs()[0] - 3.0) < 1e-9 &&
              std::abs(progress.frameTimesMs()[1] - 4.0) < 1e-9,
              "frame samples retain insertion order");
        check(progress.consumeCompletion(), "completion is delivered once after final sample");
        check(!progress.consumeCompletion(), "completion is consumed exactly once");

        progress.recordFrame(5.0);
        check(progress.frameTimesMs().size() == 2, "samples after completion are ignored");
    }
    {
        BenchmarkProgress clamped(true, 0);
        check(clamped.framesRemaining() == 1, "non-positive frame target is clamped to one frame");
    }

    if (failures == 0) std::cout << "All BenchmarkProgress tests passed.\n";
    return failures == 0 ? 0 : 1;
}
