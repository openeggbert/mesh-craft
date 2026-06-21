#pragma once
#include <atomic>
#include <future>
#include <string>

namespace MeshCraft {

// Asynchronous Claude API client.
// All public methods are safe to call from the main (UI) thread.
class AiAssistant {
public:
    std::string apiKey;
    std::string model{"claude-sonnet-4-6"};

    // Starts a background HTTPS call to the Claude API.
    // systemPrompt — role/format instructions
    // userMessage  — scene XML + user task text
    void sendAsync(const std::string& systemPrompt, const std::string& userMessage);

    bool        isInFlight() const;
    bool        isDone()     const { return done_; }
    bool        hasError()   const { return hasError_; }
    std::string result()     const { return result_; }
    std::string errorMsg()   const { return errorMsg_; }

    // Call once per frame from the UI thread to collect the background result.
    void poll();

    // Clear state so a new request can be sent.
    void reset();

private:
    std::future<std::string> future_;
    std::string              result_;
    std::string              errorMsg_;
    bool                     done_{false};
    bool                     hasError_{false};
};

} // namespace MeshCraft
