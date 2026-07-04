#pragma once
#include <atomic>
#include <future>
#include <string>
#include <utility>

namespace MeshCraft {

// Asynchronous Claude API client.
// All public methods are safe to call from the main (UI) thread.
class AiAssistant {
public:
    std::string apiKey;
    std::string model{"claude-sonnet-4-6"};
    int         maxTokens{32000};  // claude-sonnet-4-6 supports up to 64 000

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

private:
    std::future<std::pair<std::string, std::string>> future_; // {text, stop_reason}
    std::string result_;
    std::string errorMsg_;
    std::string stopReason_;
    bool        done_{false};
    bool        hasError_{false};
};

} // namespace MeshCraft
