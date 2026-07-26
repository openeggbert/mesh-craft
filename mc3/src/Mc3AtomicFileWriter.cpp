#include "MeshCraft/Mc3/Mc3AtomicFileWriter.hpp"

#include <atomic>
#include <chrono>
#include <sstream>
#include <thread>

namespace MeshCraft::Mc3 {

namespace {

std::filesystem::path candidateSiblingTempPath(const std::filesystem::path& destination,
                                                std::uint64_t nonce) {
    std::ostringstream oss;
    oss << destination.filename().string() << "." << std::hex << nonce << ".tmp";
    auto dir = destination.parent_path();
    return dir.empty() ? std::filesystem::path(oss.str()) : dir / oss.str();
}

// Bounded retry for the finalizing replace: on Windows a destination file
// can be transiently open (an antivirus scan, a slow-to-release handle from
// a prior reader) even though nothing in this process still holds it, which
// makes a single rename() attempt flaky rather than reliably atomic. POSIX
// rename() does not have this failure mode, so the loop below is a no-op
// there (it succeeds on the first attempt).
constexpr int kFinalizeRetryAttempts = 5;
constexpr std::chrono::milliseconds kFinalizeRetryDelay{20};

} // namespace

std::filesystem::path uniqueSiblingTempPath(const std::filesystem::path& destination) {
    static std::atomic<std::uint64_t> counter{0};
    for (;;) {
        const auto nonce =
            static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count()) ^
            (counter.fetch_add(1, std::memory_order_relaxed) * 0x9E3779B97F4A7C15ULL);
        auto candidate = candidateSiblingTempPath(destination, nonce);
        std::error_code ec;
        if (!std::filesystem::exists(candidate, ec))
            return candidate;
    }
}

void writeFileAtomically(
    const std::filesystem::path& destination,
    const std::function<void(const std::filesystem::path& tmpPath)>& writeFn) {
    const std::filesystem::path tmpPath = uniqueSiblingTempPath(destination);

    try {
        writeFn(tmpPath);
    } catch (...) {
        std::error_code ec;
        std::filesystem::remove(tmpPath, ec);
        throw;
    }

    std::error_code ec;
    for (int attempt = 0; attempt < kFinalizeRetryAttempts; ++attempt) {
        std::filesystem::rename(tmpPath, destination, ec);
        if (!ec) return;
        std::this_thread::sleep_for(kFinalizeRetryDelay);
    }

    const std::string message = "Failed to finalize save (rename " + tmpPath.string() +
                                 " -> " + destination.string() + "): " + ec.message();
    std::filesystem::remove(tmpPath, ec);
    throw AtomicFinalizeError(message);
}

} // namespace MeshCraft::Mc3
