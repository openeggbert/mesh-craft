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

private:
    std::future<std::pair<std::string, std::string>> future_; // {text, stop_reason}
    std::string result_;
    std::string errorMsg_;
    std::string stopReason_;
    bool        done_{false};
    bool        hasError_{false};
};

} // namespace MeshCraft
