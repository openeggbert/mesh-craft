#pragma once
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace MeshCraft {

// Shared result box for a background AI request. Heap-allocated and held by
// both AiAssistant (via a shared_ptr) and the detached worker thread's own
// shared_ptr copy — so the thread can keep running and safely write its
// result here even if the owning AiAssistant is destroyed first (STAB-0387/
// STAB-0388). Never destroyed while a thread still holds a reference to it.
struct AiRequestResult {
    std::atomic<bool> done{false};
    std::mutex        mutex; // guards the fields below
    std::string       text;
    std::string       stopReason;
    std::string       error;
    bool              hasError{false};
};

// Asynchronous Claude API client.
// All public methods are safe to call from the main (UI) thread.
class AiAssistant {
public:
    std::string apiKey;
    std::string model{"claude-sonnet-5"}; // STAB-0407: kept current with Anthropic's model lineup
    int         maxTokens{32000};  // claude-sonnet-5 supports up to 64 000

    // Base URL for the Messages API endpoint. Override for testing against a
    // local mock server (e.g. "http://127.0.0.1:PORT") — production code
    // never needs to touch this, it defaults to the real Claude API.
    std::string apiBaseUrl{"https://api.anthropic.com"};

    // HTTP timeouts, in seconds. Defaults match production needs (large
    // scenes + high max_tokens can be slow). Overridable so tests can verify
    // sendAsync() actually returns on a hung connection without waiting out
    // the full production timeout (STAB-0383/STAB-0631).
    int connectTimeoutSec{30};
    int readTimeoutSec{600};
    int writeTimeoutSec{120};

    // Starts a background HTTPS call to the Claude API with prompt caching.
    // systemPrompt — role/format instructions (cached)
    // sceneXml     — serialized mc3.xml scene sent as context (cached)
    // taskPrompt   — user's instruction for this request (not cached)
    //
    // STAB-0405: any non-200 response (429 rate limit included) is surfaced
    // once via hasError()/errorMsg() — there is no automatic retry/backoff.
    // The user must click Send again after a 429; this is a deliberate
    // simplicity choice, not an oversight.
    void sendAsync(const std::string& systemPrompt,
                   const std::string& sceneXml,
                   const std::string& taskPrompt);

    bool        isInFlight()   const;
    bool        isDone()       const { return done_; }
    bool        hasError()     const { return hasError_; }
    bool        wasTruncated() const { return stopReason_ == "max_tokens"; }
    std::string result()       const { return result_; }
    std::string errorMsg()     const { return errorMsg_; }
    std::string stopReason()   const { return stopReason_; }

    // Call once per frame from the UI thread to collect the background result.
    void poll();

    // Clear state so a new request can be sent.
    void reset();

    // --- Pure JSON helpers, exposed for direct unit testing (no CNA/ImGui/
    // network dependency) — just enough JSON handling for the Claude API's
    // request/response shape. ---
    static std::string jsonEscape(const std::string& s);
    static std::string extractStopReason(const std::string& json);
    static std::string extractFirstTextValue(const std::string& json);

    // SYS-W2-04: redacts every occurrence of `secret` in `text`, replacing
    // it with "[REDACTED]" -- defense-in-depth so the API key can never
    // appear in an error message/log even if a future change accidentally
    // interpolates a request header into one (today's error paths don't,
    // but nothing structurally prevented that). No-op if `secret` is empty
    // (never redacts everything to "[REDACTED]").
    static std::string redactSecret(std::string text, const std::string& secret);

    // SYS-W2-04: truncates `text` to at most `maxLen` bytes, appending a
    // "... (truncated, N bytes total)" note, so an oversized or malicious
    // HTTP response body can't propagate an unbounded string into an error
    // message or the UI. Does NOT shrink the buffer httplib itself already
    // holds in memory for the response (this httplib version has no Post()
    // overload that streams the response through a size-capping
    // ContentReceiver) -- that remains a known gap, tracked in plan.md.
    static std::string boundedForDisplay(const std::string& text, size_t maxLen = 4096);

    // AUD-014: sendAsync() detaches its network worker thread (see reset()'s
    // STAB-0387/0388 comment for why it must not block the UI thread). That
    // means nothing stops the process from returning from main() while a
    // worker is still executing cpp-httplib/OpenSSL code -- static
    // destructors and OpenSSL's own atexit cleanup would then race a live
    // thread still inside that library, a classic exit-time UB/crash. Call
    // this once, right before the process would otherwise exit, to block
    // (bounded by `timeout`, NOT indefinitely -- an unbounded wait here
    // would reintroduce the exact hang STAB-0387/0388 removed from reset())
    // until every in-flight sendAsync() worker across every AiAssistant
    // instance has finished. Returns true if all workers finished within
    // the timeout, false if the timeout elapsed with one or more still
    // running (those are abandoned exactly as before this fix -- no
    // regression for the already-hung case, but a request that was moments
    // from finishing now gets to finish cleanly instead of racing teardown).
    static bool waitForAllInFlight(std::chrono::milliseconds timeout);

private:
    std::shared_ptr<AiRequestResult> pending_; // null when no request has ever been sent
    std::string result_;
    std::string errorMsg_;
    std::string stopReason_;
    bool        done_{false};
    bool        hasError_{false};
};

} // namespace MeshCraft
