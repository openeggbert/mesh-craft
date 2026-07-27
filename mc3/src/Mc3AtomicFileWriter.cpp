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
//
// Real windows-2022 CI observed this exceeding a 5x20ms (100ms) budget for
// a multi-script Unicode filename (STAB-0555's Czech+Japanese roundtrip
// case) with "Input/output error" -- confirmed via a faithful MinGW+Wine
// repro that the path/encoding itself round-trips correctly (libstdc++'s
// char8_t path constructor already guarantees UTF-8, independent of
// locale), so the failure is a genuinely slower transient lock on that
// runner (e.g. Defender's real-time scan taking longer on an unusual
// filename), not a string-corruption bug. Widened to give real Windows AV
// scans more headroom; still a no-op on the fast path everywhere else.
constexpr int kFinalizeRetryAttempts = 20;
constexpr std::chrono::milliseconds kFinalizeRetryDelay{50};

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
