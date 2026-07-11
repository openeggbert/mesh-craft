#include "MeshCraft/AiAssistant.hpp"

// httplib.h is available when MESHCRAFT_HAS_AI is defined (CMake sets this
// when both cpp-httplib and OpenSSL are found).
#ifdef MESHCRAFT_HAS_AI
#  ifdef CPPHTTPLIB_OPENSSL_SUPPORT
// Already defined via CMake / httplib::httplib target
#  endif
#  include <httplib.h>
#endif

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace MeshCraft {

// AUD-014: process-wide count of currently-executing sendAsync() worker
// threads, so waitForAllInFlight() (called once at process exit) can block
// until it reaches zero. Incremented synchronously on the CALLING thread
// before the worker std::thread is even constructed (not inside the worker
// lambda) -- incrementing inside the lambda would leave a race window
// between std::thread's constructor returning and the new thread's first
// instruction actually running, during which waitForAllInFlight() could
// observe count==0 and return immediately despite a worker being about to
// start real (httplib/OpenSSL) work.
namespace {
std::mutex              g_inFlightMutex;
std::condition_variable g_inFlightCv;
int                     g_inFlightCount = 0;

void beginAiWorker() {
    std::lock_guard<std::mutex> lock(g_inFlightMutex);
    ++g_inFlightCount;
}

// RAII: decrements + notifies on every exit path from the worker lambda
// (normal return or the lambda's own catch already turned any exception
// into the hasError/error fields, so this always runs on scope exit).
struct AiWorkerScopeGuard {
    ~AiWorkerScopeGuard() {
        {
            std::lock_guard<std::mutex> lock(g_inFlightMutex);
            --g_inFlightCount;
        }
        g_inFlightCv.notify_all();
    }
};
} // namespace

bool AiAssistant::waitForAllInFlight(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(g_inFlightMutex);
    return g_inFlightCv.wait_for(lock, timeout, [] { return g_inFlightCount == 0; });
}

// ---------------------------------------------------------------------------
// JSON helpers — just enough for the Claude API request/response
// (declared as public static members in AiAssistant.hpp so ai_test.cpp can
// exercise them directly, no CNA/ImGui/network dependency)
// ---------------------------------------------------------------------------

std::string AiAssistant::jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + s.size() / 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

// Extract the value of "stop_reason" from a Claude /v1/messages response JSON.
// Returns "end_turn", "max_tokens", or empty string if not found.
std::string AiAssistant::extractStopReason(const std::string& json) {
    const std::string key = "\"stop_reason\":\"";
    auto pos = json.find(key);
    if (pos == std::string::npos) return {};
    pos += key.size();
    std::string out;
    while (pos < json.size() && json[pos] != '"')
        out += json[pos++];
    return out;
}

// Extract the text value of the first "text" key in the JSON response.
// Claude /v1/messages response: {"content":[{"type":"text","text":"..."}]}
std::string AiAssistant::extractFirstTextValue(const std::string& json) {
    const std::string key = "\"text\":\"";
    auto pos = json.find(key);
    if (pos == std::string::npos) return {};
    pos += key.size();
    std::string out;
    while (pos < json.size()) {
        char c = json[pos];
        if (c == '\\' && pos + 1 < json.size()) {
            char n = json[pos + 1];
            switch (n) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'u':
                    if (pos + 5 < json.size()) {
                        unsigned code = 0;
                        for (int i = 2; i <= 5; ++i) {
                            char h = json[pos + i];
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= h - '0';
                            else if (h >= 'a' && h <= 'f') code |= h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') code |= h - 'A' + 10;
                        }
                        // Encode as UTF-8 (BMP only)
                        if (code < 0x80) {
                            out += static_cast<char>(code);
                        } else if (code < 0x800) {
                            out += static_cast<char>(0xC0 | (code >> 6));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (code >> 12));
                            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        pos += 4; // extra 4 (plus the 2 from outer +=2)
                    }
                    break;
                default: out += n; break;
            }
            pos += 2;
        } else if (c == '"') {
            break;
        } else {
            out += c;
            ++pos;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// AiAssistant implementation
// ---------------------------------------------------------------------------

bool AiAssistant::isInFlight() const {
    return pending_ != nullptr && !done_;
}

void AiAssistant::poll() {
    if (!pending_ || done_) return;
    if (!pending_->done.load(std::memory_order_acquire)) return;

    std::lock_guard<std::mutex> lock(pending_->mutex);
    if (pending_->hasError) {
        result_     = {};
        stopReason_ = {};
        errorMsg_   = pending_->error;
        hasError_   = true;
    } else {
        result_     = pending_->text;
        stopReason_ = pending_->stopReason;
        hasError_   = false;
    }
    done_ = true;
}

void AiAssistant::reset() {
    // STAB-0387/STAB-0388: previously this blocked (via future_.wait()) until
    // an in-flight request finished — since future_ was a std::async future,
    // its *destructor* (e.g. app shutdown while a request is in flight) had
    // the exact same blocking behavior, up to readTimeoutSec (600s default).
    // Now pending_ just drops our reference to the shared result box; the
    // background thread (detached, holding its own shared_ptr copy) keeps
    // running independently and safely writes into it whether or not this
    // AiAssistant (or the whole app) still exists — no blocking anywhere.
    pending_.reset();
    result_.clear();
    errorMsg_.clear();
    stopReason_.clear();
    done_     = false;
    hasError_ = false;
}

void AiAssistant::sendAsync(const std::string& systemPrompt,
                            const std::string& sceneXml,
                            const std::string& taskPrompt) {
    reset();
    std::string apiKeyCopy    = apiKey;
    std::string modelCopy     = model;
    int         maxTokensCopy = maxTokens;
    std::string baseUrlCopy   = apiBaseUrl;
    int         connectTimeoutCopy = connectTimeoutSec;
    int         readTimeoutCopy    = readTimeoutSec;
    int         writeTimeoutCopy   = writeTimeoutSec;

    auto pending = std::make_shared<AiRequestResult>();
    pending_ = pending;

    beginAiWorker(); // see AUD-014 comment above -- must happen before the thread starts
    std::thread([pending, apiKeyCopy, modelCopy, maxTokensCopy, baseUrlCopy,
                 connectTimeoutCopy, readTimeoutCopy, writeTimeoutCopy,
                 systemPrompt, sceneXml, taskPrompt]()
    {
        AiWorkerScopeGuard workerGuard;
        std::string text, stopReason, error;
        bool hasError = false;
        try {
#ifdef MESHCRAFT_HAS_AI
            // httplib::Client parses the scheme from baseUrlCopy and internally
            // dispatches to an SSL-backed client for "https://" (production,
            // default apiBaseUrl) or a plain socket for "http://" (test mock
            // servers point apiBaseUrl at http://127.0.0.1:PORT).
            httplib::Client cli(baseUrlCopy);
            cli.set_connection_timeout(connectTimeoutCopy, 0);
            cli.set_read_timeout(readTimeoutCopy, 0);
            cli.set_write_timeout(writeTimeoutCopy, 0);

            // Structured request with prompt caching:
            //   system prompt  → cached (stable across requests)
            //   scene XML      → cached (stable while editing same scene)
            //   task prompt    → NOT cached (changes every request)
            std::string body =
                "{"
                "\"model\":\"" + AiAssistant::jsonEscape(modelCopy) + "\","
                "\"max_tokens\":" + std::to_string(maxTokensCopy) + ","
                "\"system\":[{"
                  "\"type\":\"text\","
                  "\"text\":\"" + AiAssistant::jsonEscape(systemPrompt) + "\","
                  "\"cache_control\":{\"type\":\"ephemeral\"}"
                "}],"
                "\"messages\":[{\"role\":\"user\",\"content\":["
                  "{"
                    "\"type\":\"text\","
                    "\"text\":\"" + AiAssistant::jsonEscape(sceneXml) + "\","
                    "\"cache_control\":{\"type\":\"ephemeral\"}"
                  "},"
                  "{"
                    "\"type\":\"text\","
                    "\"text\":\"Task: " + AiAssistant::jsonEscape(taskPrompt) + "\""
                  "}"
                "]}]}";

            httplib::Headers headers = {
                {"x-api-key",          apiKeyCopy},
                {"anthropic-version",  "2023-06-01"},
                {"anthropic-beta",     "prompt-caching-2024-07-31"},
                {"content-type",       "application/json"}
            };

            auto res = cli.Post("/v1/messages", headers, body, "application/json");
            if (!res) {
                throw std::runtime_error("HTTP request failed: " +
                    httplib::to_string(res.error()));
            }
            if (res->status != 200) {
                throw std::runtime_error("API error " + std::to_string(res->status) +
                    ": " + res->body);
            }

            stopReason = AiAssistant::extractStopReason(res->body);
            text       = AiAssistant::extractFirstTextValue(res->body);
            if (text.empty()) {
                throw std::runtime_error("Empty response from API: " + res->body);
            }
#else
            (void)apiKeyCopy; (void)modelCopy; (void)maxTokensCopy; (void)baseUrlCopy;
            (void)systemPrompt; (void)sceneXml; (void)taskPrompt;
            throw std::runtime_error(
                "AI not available: built without cpp-httplib + OpenSSL.\n"
                "Install libssl-dev and reconfigure.");
#endif
        } catch (const std::exception& e) {
            hasError = true;
            error    = e.what();
        }

        {
            std::lock_guard<std::mutex> lock(pending->mutex);
            pending->text       = std::move(text);
            pending->stopReason = std::move(stopReason);
            pending->error      = std::move(error);
            pending->hasError   = hasError;
        }
        pending->done.store(true, std::memory_order_release);
    }).detach();
}

} // namespace MeshCraft
