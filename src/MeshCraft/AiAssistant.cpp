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
#include <cstdio>
#include <future>
#include <stdexcept>
#include <string>

namespace MeshCraft {

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
    return future_.valid() && !done_;
}

void AiAssistant::poll() {
    if (!future_.valid() || done_) return;
    if (future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto [text, stop] = future_.get();
            result_     = std::move(text);
            stopReason_ = std::move(stop);
            done_       = true;
            hasError_   = false;
        } catch (const std::exception& e) {
            result_     = {};
            stopReason_ = {};
            errorMsg_   = e.what();
            done_       = true;
            hasError_   = true;
        }
    }
}

void AiAssistant::reset() {
    if (future_.valid() && !done_)
        future_.wait(); // wait for in-flight call to finish before resetting
    future_ = {};
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

    future_ = std::async(std::launch::async,
        [apiKeyCopy, modelCopy, maxTokensCopy, baseUrlCopy,
         connectTimeoutCopy, readTimeoutCopy, writeTimeoutCopy,
         systemPrompt, sceneXml, taskPrompt]()
            -> std::pair<std::string, std::string>   // {text, stop_reason}
    {
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
        if (!res)
            throw std::runtime_error("HTTP request failed: " +
                httplib::to_string(res.error()));
        if (res->status != 200)
            throw std::runtime_error("API error " + std::to_string(res->status) +
                ": " + res->body);

        std::string stopReason = AiAssistant::extractStopReason(res->body);
        std::string text       = AiAssistant::extractFirstTextValue(res->body);
        if (text.empty())
            throw std::runtime_error("Empty response from API: " + res->body);
        return {text, stopReason};
#else
        (void)apiKeyCopy; (void)modelCopy; (void)maxTokensCopy; (void)baseUrlCopy;
        (void)systemPrompt; (void)sceneXml; (void)taskPrompt;
        throw std::runtime_error(
            "AI not available: built without cpp-httplib + OpenSSL.\n"
            "Install libssl-dev and reconfigure.");
#endif
    });
}

} // namespace MeshCraft
