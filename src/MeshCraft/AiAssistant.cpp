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
// Returns "end_turn", "max_tokens", or empty string if not found (including
// a literal `"stop_reason":null` -- that and "not present at all" are not
// distinguished, but wasTruncated() only ever compares against "max_tokens",
// so both collapse to the same "not truncated" outcome either way).
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

namespace {

// Appends `code` (a full Unicode code point, already surrogate-pair-
// combined if it came from one) to `out` as UTF-8. A code point still in
// the surrogate range (0xD800-0xDFFF) here is always a LONE, unpaired
// surrogate -- extractFirstTextValue() below already combines valid pairs
// before ever reaching this function -- so it is replaced with U+FFFD (the
// standard Unicode "invalid sequence" replacement character) instead of
// being encoded as an invalid 3-byte pseudo-UTF-8 sequence.
void appendUtf8CodePoint(std::string& out, unsigned code) {
    if (code >= 0xD800 && code <= 0xDFFF) code = 0xFFFD;
    if (code < 0x80) {
        out += static_cast<char>(code);
    } else if (code < 0x800) {
        out += static_cast<char>(0xC0 | (code >> 6));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
        out += static_cast<char>(0xE0 | (code >> 12));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (code >> 18));
        out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (code & 0x3F));
    }
}

// Reads the 4 hex digits of a `\uXXXX` escape, where `pos` is the index of
// the backslash. Returns -1 if out of bounds or not valid hex.
long readHex4(const std::string& json, size_t pos) {
    if (pos + 5 >= json.size()) return -1;
    unsigned code = 0;
    for (int i = 2; i <= 5; ++i) {
        char h = json[pos + i];
        code <<= 4;
        if      (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
        else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
        else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
        else return -1;
    }
    return static_cast<long>(code);
}

} // namespace

// Extract the text value from the first {"type":"text","text":"..."} block
// in a Claude /v1/messages response, e.g.
// {"content":[{"type":"text","text":"..."}],"stop_reason":"end_turn"}.
//
// AUD-009: previously searched for the bare "text":" key anywhere in the
// body -- Claude's Messages API can emit other content-block types
// (citations, tool_use, future server-tool blocks) that might themselves
// carry an unrelated "text"-named field earlier in the response, which
// would have been extracted instead of the real answer. Anchoring on the
// full "type":"text","text":" prefix scopes this to an actual text block,
// without introducing a real JSON parser dependency (a deliberate,
// documented scope choice for this file) -- it assumes compact JSON with no
// inter-token whitespace, which every other hand-rolled scan here already
// assumes (this is how api.anthropic.com actually serializes responses).
std::string AiAssistant::extractFirstTextValue(const std::string& json) {
    const std::string key = "\"type\":\"text\",\"text\":\"";
    auto pos = json.find(key);
    if (pos == std::string::npos) return {};
    pos += key.size();

    std::string out;
    while (pos < json.size()) {
        char c = json[pos];
        if (c == '"') break;
        if (c != '\\' || pos + 1 >= json.size()) { out += c; ++pos; continue; }

        char n = json[pos + 1];
        if (n == 'u') {
            long code = readHex4(json, pos);
            if (code < 0) { pos += 2; continue; } // malformed escape, skip just the "\u"

            if (code >= 0xD800 && code <= 0xDBFF) {
                // Possible high surrogate: combine with an immediately
                // following low surrogate into one astral code point
                // (AUD-009 -- previously each half was UTF-8-encoded
                // independently, producing two invalid sequences for any
                // character outside the Basic Multilingual Plane, e.g.
                // most emoji).
                size_t lowPos = pos + 6; // just past this "\uXXXX"
                long low = (lowPos + 1 < json.size() && json[lowPos] == '\\' && json[lowPos + 1] == 'u')
                               ? readHex4(json, lowPos) : -1;
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    unsigned astral = 0x10000u
                        + ((static_cast<unsigned>(code) - 0xD800u) << 10)
                        + (static_cast<unsigned>(low) - 0xDC00u);
                    appendUtf8CodePoint(out, astral);
                    pos = lowPos + 6; // consumed both "\uXXXX" escapes
                    continue;
                }
                // Lone high surrogate -- no valid low surrogate follows;
                // falls through to appendUtf8CodePoint(code) below, which
                // maps it to U+FFFD.
            }
            appendUtf8CodePoint(out, static_cast<unsigned>(code));
            pos += 6;
            continue;
        }

        switch (n) {
            case '"':  out += '"';  break;
            case '\\': out += '\\'; break;
            case '/':  out += '/';  break;
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            case 't':  out += '\t'; break;
            case 'b':  out += '\b'; break;
            case 'f':  out += '\f'; break;
            default:   out += n;    break;
        }
        pos += 2;
    }
    return out;
}

std::string AiAssistant::redactSecret(std::string text, const std::string& secret) {
    if (secret.empty()) return text;
    static constexpr const char* kRedacted = "[REDACTED]";
    size_t pos = 0;
    while ((pos = text.find(secret, pos)) != std::string::npos) {
        text.replace(pos, secret.size(), kRedacted);
        pos += std::char_traits<char>::length(kRedacted);
    }
    return text;
}

std::string AiAssistant::boundedForDisplay(const std::string& text, size_t maxLen) {
    if (text.size() <= maxLen) return text;
    return text.substr(0, maxLen) + "... (truncated, " +
           std::to_string(text.size()) + " bytes total)";
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
    size_t      maxResponseBytesCopy = maxResponseBytes;

    auto pending = std::make_shared<AiRequestResult>();
    pending_ = pending;

    beginAiWorker(); // see AUD-014 comment above -- must happen before the thread starts
    std::thread([pending, apiKeyCopy, modelCopy, maxTokensCopy, baseUrlCopy,
                 connectTimeoutCopy, readTimeoutCopy, writeTimeoutCopy,
                 maxResponseBytesCopy, systemPrompt, sceneXml, taskPrompt]()
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

            // SYS-W2-05: cap the response body size DURING the network read
            // itself (SYS-W2-04 already bounds how much of it can appear in
            // an error message, but that alone doesn't stop httplib from
            // buffering an unbounded body in memory first). This httplib
            // version's Post() convenience wrapper has no response-
            // streaming overload -- only Get() does -- so a raw Request is
            // built here and sent via Client::send(), mirroring exactly
            // what Post(path, headers, body, content_type) does internally
            // (see send_with_content_provider()) plus a content_receiver
            // that aborts the read once the cap is exceeded.
            std::string responseBody;
            bool responseTooLarge = false;
            httplib::Request req;
            req.method  = "POST";
            req.path    = "/v1/messages";
            req.headers = headers;
            req.body    = body;
            req.content_receiver = [&](const char* data, size_t len, uint64_t, uint64_t) -> bool {
                if (responseBody.size() + len > maxResponseBytesCopy) {
                    responseTooLarge = true;
                    return false; // aborts the connection/read
                }
                responseBody.append(data, len);
                return true;
            };

            auto res = cli.send(req);
            if (!res) {
                if (responseTooLarge) {
                    throw std::runtime_error("API response exceeded the " +
                        std::to_string(maxResponseBytesCopy) + "-byte cap and was aborted");
                }
                throw std::runtime_error("HTTP request failed: " +
                    httplib::to_string(res.error()));
            }
            // SYS-W2-04: bound how much of a (possibly huge or malicious,
            // e.g. from a user-pointed apiBaseUrl) response body can
            // propagate into an error message/the UI.
            if (res->status != 200) {
                throw std::runtime_error("API error " + std::to_string(res->status) +
                    ": " + AiAssistant::boundedForDisplay(responseBody));
            }

            stopReason = AiAssistant::extractStopReason(responseBody);
            text       = AiAssistant::extractFirstTextValue(responseBody);
            if (text.empty()) {
                throw std::runtime_error("Empty response from API: " +
                    AiAssistant::boundedForDisplay(responseBody));
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
            // SYS-W2-04: defense-in-depth -- no current path above
            // interpolates apiKeyCopy into a message, but nothing
            // structurally prevented that either, so redact just in case.
            error    = AiAssistant::redactSecret(e.what(), apiKeyCopy);
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
