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
// ---------------------------------------------------------------------------

static std::string jsonEscape(const std::string& s) {
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

// Extract the text value of the first "text" key in the JSON response.
// Claude /v1/messages response: {"content":[{"type":"text","text":"..."}]}
static std::string extractFirstTextValue(const std::string& json) {
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
            result_   = future_.get();
            done_     = true;
            hasError_ = false;
        } catch (const std::exception& e) {
            result_   = {};
            errorMsg_ = e.what();
            done_     = true;
            hasError_ = true;
        }
    }
}

void AiAssistant::reset() {
    if (future_.valid() && !done_)
        future_.wait(); // wait for in-flight call to finish before resetting
    future_ = {};
    result_.clear();
    errorMsg_.clear();
    done_     = false;
    hasError_ = false;
}

void AiAssistant::sendAsync(const std::string& systemPrompt, const std::string& userMessage) {
    reset();
    std::string apiKeyCopy = apiKey;
    std::string modelCopy  = model;

    future_ = std::async(std::launch::async,
        [apiKeyCopy, modelCopy, systemPrompt, userMessage]() -> std::string
    {
#ifdef MESHCRAFT_HAS_AI
        httplib::SSLClient cli("api.anthropic.com");
        cli.set_connection_timeout(30, 0);
        cli.set_read_timeout(120, 0);
        cli.set_write_timeout(30, 0);

        std::string body =
            "{\"model\":\"" + jsonEscape(modelCopy) + "\","
            "\"max_tokens\":8192,"
            "\"system\":\"" + jsonEscape(systemPrompt) + "\","
            "\"messages\":[{\"role\":\"user\","
            "\"content\":\"" + jsonEscape(userMessage) + "\"}]}";

        httplib::Headers headers = {
            {"x-api-key",          apiKeyCopy},
            {"anthropic-version",  "2023-06-01"},
            {"content-type",       "application/json"}
        };

        auto res = cli.Post("/v1/messages", headers, body, "application/json");
        if (!res)
            throw std::runtime_error("HTTP request failed: " +
                httplib::to_string(res.error()));
        if (res->status != 200)
            throw std::runtime_error("API error " + std::to_string(res->status) +
                ": " + res->body);

        std::string text = extractFirstTextValue(res->body);
        if (text.empty())
            throw std::runtime_error("Empty response from API: " + res->body);
        return text;
#else
        (void)apiKeyCopy; (void)modelCopy;
        (void)systemPrompt; (void)userMessage;
        throw std::runtime_error(
            "AI not available: built without cpp-httplib + OpenSSL.\n"
            "Install libssl-dev and reconfigure.");
#endif
    });
}

} // namespace MeshCraft
