// Gate 4 — AI mock tests (STAB-0371..0376).
//
// Covers two layers, both CNA/ImGui-free:
//  1. AiAssistant's pure JSON helpers (AiAssistant::jsonEscape/
//     extractStopReason/extractFirstTextValue) and, when MESHCRAFT_HAS_AI is
//     available, a full sendAsync()/poll() round-trip against a local mock
//     HTTP server (AiAssistant::apiBaseUrl is overridable for exactly this).
//  2. The AI-response validation pipeline mirrored in
//     AiResponseAlgorithms.hpp (extractXmlAlg/repairXmlAlg/parseXmlAlg/
//     isEmptyMc3DocumentAlg/validateAndParseAiResponseAlg), which
//     MeshCraftApplication_UiAi.cpp calls directly (not a duplicate).

#include "MeshCraft/AiAssistant.hpp"
#include "AiResponseAlgorithms.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#ifdef MESHCRAFT_HAS_AI
#include <httplib.h>
#endif

using namespace MeshCraft;
using namespace MeshCraft::Mc3;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while(0)

// ─────────────────────────────────────────────────────────────────────────────
// AiAssistant::jsonEscape / extractStopReason / extractFirstTextValue
// (compiled unconditionally — these never depended on MESHCRAFT_HAS_AI)
// ─────────────────────────────────────────────────────────────────────────────

static void testJsonEscape() {
    CHECK(AiAssistant::jsonEscape("plain") == "plain", "jsonEscape: plain text unchanged");
    CHECK(AiAssistant::jsonEscape("say \"hi\"") == "say \\\"hi\\\"",
          "jsonEscape: double quotes escaped");
    CHECK(AiAssistant::jsonEscape("back\\slash") == "back\\\\slash",
          "jsonEscape: backslash escaped");
    CHECK(AiAssistant::jsonEscape("line1\nline2") == "line1\\nline2",
          "jsonEscape: newline escaped");
    CHECK(AiAssistant::jsonEscape("tab\there") == "tab\\there", "jsonEscape: tab escaped");
    std::string ctrl(1, '\x01');
    CHECK(AiAssistant::jsonEscape(ctrl) == "\\u0001",
          "jsonEscape: other control characters escaped as \\u00XX");
}

static void testExtractStopReason() {
    CHECK(AiAssistant::extractStopReason(R"({"stop_reason":"end_turn"})") == "end_turn",
          "extractStopReason: finds end_turn");
    CHECK(AiAssistant::extractStopReason(R"({"stop_reason":"max_tokens"})") == "max_tokens",
          "extractStopReason: finds max_tokens");
    CHECK(AiAssistant::extractStopReason(R"({"no_such_key":"x"})").empty(),
          "extractStopReason: missing key returns empty string");
}

static void testExtractFirstTextValue() {
    CHECK(AiAssistant::extractFirstTextValue(R"({"content":[{"type":"text","text":"hello"}]})")
              == "hello",
          "extractFirstTextValue: extracts plain text");
    CHECK(AiAssistant::extractFirstTextValue(R"({"content":[{"type":"text","text":"a\nb"}]})")
              == "a\nb",
          "extractFirstTextValue: unescapes \\n");
    CHECK(AiAssistant::extractFirstTextValue(R"({"content":[{"type":"text","text":"a\"b"}]})")
              == "a\"b",
          "extractFirstTextValue: unescapes \\\"");
    std::string jsonWithUnicodeEscape =
        std::string(R"({"content":[{"type":"text","text":"caf)") + "\\u00e9" + R"("}]})";
    CHECK(AiAssistant::extractFirstTextValue(jsonWithUnicodeEscape) == "caf\xC3\xA9",
          "extractFirstTextValue: decodes a \\u00e9 JSON escape to UTF-8 (2-byte case)");
    CHECK(AiAssistant::extractFirstTextValue(R"({"no_text_key":1})").empty(),
          "extractFirstTextValue: missing key returns empty string");
}

// ─────────────────────────────────────────────────────────────────────────────
// extractXmlAlg (STAB-0372/0373/0374)
// ─────────────────────────────────────────────────────────────────────────────

static void testExtractXmlPlain() {
    std::string xml = "<mc3 version=\"0.3\"><objects/></mc3>";
    CHECK(extractXmlAlg(xml) == xml,
          "STAB-0372: a response that's already plain XML is returned unchanged");

    std::string withDecl = "<?xml version=\"1.0\"?><mc3><objects/></mc3>";
    CHECK(extractXmlAlg(withDecl) == withDecl,
          "extractXmlAlg: a response starting with <?xml is returned unchanged too");
}

static void testExtractXmlStripsMarkdownFence() {
    std::string wrapped = "```xml\n<mc3 version=\"0.3\"><objects/></mc3>\n```";
    std::string extracted = extractXmlAlg(wrapped);
    CHECK(extracted == "<mc3 version=\"0.3\"><objects/></mc3>",
          "STAB-0373: both the leading ```xml fence and the trailing ``` fence are stripped, "
          "leaving exactly the XML");
}

static void testExtractXmlWithSurroundingProse() {
    std::string wrapped = "Here is the scene: <mc3 version=\"0.3\"><objects/></mc3> Hope that helps";
    std::string extracted = extractXmlAlg(wrapped);
    CHECK(extracted == "<mc3 version=\"0.3\"><objects/></mc3>",
          "STAB-0374: both leading and trailing prose around the XML are stripped");
}

// This is a real bug found while writing these tests: the original
// extractXmlAlg only located the START marker and returned everything to
// the end of the string, so a markdown-fenced or prose-trailed response
// kept its trailing garbage. Mc3Document::loadFromFile()/tinyxml2 does NOT
// tolerate content after the root element closes — confirmed by first
// writing this test against the un-fixed extractXmlAlg and watching it
// fail here, not just in the isolated extraction tests above. Fixed by
// having extractXmlAlg also trim at the matching "</mc3>". These two cases
// are exactly what the original AI-panel system prompt's "no prose, no
// markdown fences" instruction exists to avoid — but LLMs don't always
// follow that instruction perfectly, so this fallback matters for
// real-world reliability, not just hypothetical malformed input.
static void testFullPipelineHandlesMarkdownFenceAndProse() {
    // Non-empty scene content (a box) — deliberately distinct from the
    // empty-<objects/> case, so a failure here can't be confused with the
    // unrelated STAB-0376 empty-document rejection.
    std::string fenced =
        "```xml\n<mc3 version=\"0.3\"><objects><box id=\"b1\"/></objects></mc3>\n```";
    auto fencedResult = validateAndParseAiResponseAlg(fenced);
    CHECK(fencedResult.doc.has_value(),
          "STAB-0373: a markdown-fenced response parses successfully end-to-end");
    if (fencedResult.doc)
        CHECK(fencedResult.doc->objects.size() == 1,
              "STAB-0373: the fenced response's object survives the full pipeline intact");

    std::string prose = "Here is the scene: "
        "<mc3 version=\"0.3\"><objects><box id=\"b1\"/></objects></mc3> Hope that helps";
    auto proseResult = validateAndParseAiResponseAlg(prose);
    CHECK(proseResult.doc.has_value(),
          "STAB-0374: a response with surrounding prose parses successfully end-to-end");
    if (proseResult.doc)
        CHECK(proseResult.doc->objects.size() == 1,
              "STAB-0374: the prose-wrapped response's object survives the full pipeline intact");
}

// ─────────────────────────────────────────────────────────────────────────────
// repairXmlAlg
// ─────────────────────────────────────────────────────────────────────────────

static void testRepairXmlInsertsMissingCloseBracket() {
    // A common AI mistake: an opening tag's '>' is missing before a child
    // starts (here, separated by a newline — the realistic case: the AI
    // wrote the attribute list, pressed enter, and forgot the '>').
    std::string broken = "<mc3 version=\"0.3\"><group name=\"x\"\n<child/></group></mc3>";
    std::string repaired = repairXmlAlg(broken);
    auto childPos = repaired.find("<child/>");
    CHECK(childPos != std::string::npos && childPos > 0 && repaired[childPos - 1] == '>',
          "repairXmlAlg: injects the missing '>' immediately before the next tag starts");
    CHECK(repaired.find("<<") == std::string::npos,
          "repairXmlAlg: never leaves two '<' adjacent — the injected '>' always separates them");

    // Well-formed input is left unchanged.
    std::string ok = "<mc3 version=\"0.3\"><objects/></mc3>";
    CHECK(repairXmlAlg(ok) == ok, "repairXmlAlg: well-formed XML passes through unchanged");
}

// ─────────────────────────────────────────────────────────────────────────────
// Full validation pipeline (STAB-0375/0376)
// ─────────────────────────────────────────────────────────────────────────────

static void testValidateAndParseInvalidXmlSetsError() {
    auto result = validateAndParseAiResponseAlg("not xml here, sorry");
    CHECK(!result.doc.has_value(),
          "STAB-0375: a response with no <mc3> root produces no document");
    CHECK(!result.errorMessage.empty(),
          "STAB-0375: a non-empty error message is set instead");
    CHECK(result.errorMessage.find("<mc3> root") != std::string::npos,
          "STAB-0375: the error message identifies the missing <mc3> root specifically");
}

static void testValidateAndParseMalformedXmlSetsError() {
    // Has "<mc3" so it passes the root-marker check, but isn't valid XML.
    auto result = validateAndParseAiResponseAlg("<mc3 version=\"0.3\"><objects>");
    CHECK(!result.doc.has_value(),
          "STAB-0375: malformed (unclosed) XML produces no document");
    CHECK(!result.errorMessage.empty(),
          "STAB-0375: a non-empty error message is set for malformed XML too");
}

static void testValidateAndParseEmptyDocumentRejected() {
    auto result = validateAndParseAiResponseAlg("<mc3 version=\"0.3\"></mc3>");
    CHECK(!result.doc.has_value(),
          "STAB-0376: a document with no objects and no definitions is rejected");
    CHECK(result.errorMessage.find("empty document") != std::string::npos,
          "STAB-0376: the error message explains it's an empty-document rejection");
}

static void testValidateAndParseAcceptsNonEmptyDocument() {
    auto result = validateAndParseAiResponseAlg(
        "<mc3 version=\"0.3\"><objects><box id=\"b1\"/></objects></mc3>");
    CHECK(result.doc.has_value(),
          "validateAndParseAiResponseAlg: a document with at least one object is accepted");
    if (result.doc)
        CHECK(result.doc->objects.size() == 1,
              "validateAndParseAiResponseAlg: the accepted document has the expected object");
}

// ─────────────────────────────────────────────────────────────────────────────
// STAB-0371 — mock HTTP server round-trip through the real AiAssistant
// ─────────────────────────────────────────────────────────────────────────────

#ifdef MESHCRAFT_HAS_AI

static bool pollUntilDone(AiAssistant& ai, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!ai.isDone()) {
        ai.poll();
        if (ai.isDone()) break;
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

static void waitUntilServerRunning(httplib::Server& svr) {
    while (!svr.is_running())
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
}

static void testMockServerSuccessRoundTrip() {
    httplib::Server svr;
    svr.Post("/v1/messages", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(
            R"({"content":[{"type":"text","text":"<mc3 version=\"0.3\"><objects><box id=\"b1\"/></objects></mc3>"}],"stop_reason":"end_turn"})",
            "application/json");
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::thread serverThread([&]{ svr.listen_after_bind(); });
    waitUntilServerRunning(svr);

    AiAssistant ai;
    ai.apiKey     = "test-key";
    ai.apiBaseUrl = "http://127.0.0.1:" + std::to_string(port);
    ai.sendAsync("system prompt", "<mc3/>", "add a box");

    bool finished = pollUntilDone(ai, 5000);
    CHECK(finished, "mock server: request completes within the timeout");
    CHECK(!ai.hasError(), "mock server: no error reported");
    CHECK(ai.result().find("<mc3") != std::string::npos,
          "STAB-0371: AiAssistant extracts the XML text from a mock HTTP response");
    CHECK(ai.stopReason() == "end_turn", "mock server: stop_reason extracted correctly");
    CHECK(!ai.wasTruncated(), "mock server: end_turn is not treated as truncation");

    auto validated = validateAndParseAiResponseAlg(ai.result());
    CHECK(validated.doc.has_value(),
          "STAB-0371: the mock server's response, once run through the full "
          "AI panel pipeline (AiAssistant -> validateAndParseAiResponseAlg), "
          "produces a usable document — end-to-end, no real network call");

    svr.stop();
    serverThread.join();
}

static void testMockServerTruncatedResponse() {
    httplib::Server svr;
    svr.Post("/v1/messages", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(
            R"({"content":[{"type":"text","text":"<mc3 version=\"0.3\"><objects>"}],"stop_reason":"max_tokens"})",
            "application/json");
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::thread serverThread([&]{ svr.listen_after_bind(); });
    waitUntilServerRunning(svr);

    AiAssistant ai;
    ai.apiKey     = "test-key";
    ai.apiBaseUrl = "http://127.0.0.1:" + std::to_string(port);
    ai.sendAsync("system prompt", "<mc3/>", "add a box");

    bool finished = pollUntilDone(ai, 5000);
    CHECK(finished, "mock server (truncated): request completes within the timeout");
    CHECK(!ai.hasError(), "mock server (truncated): the HTTP call itself succeeded");
    CHECK(ai.wasTruncated(),
          "mock server (truncated): stop_reason=max_tokens is correctly reported as truncated");

    svr.stop();
    serverThread.join();
}

static void testMockServerHttpErrorStatus() {
    httplib::Server svr;
    svr.Post("/v1/messages", [](const httplib::Request&, httplib::Response& res) {
        res.status = 401;
        res.set_content(R"({"error":{"message":"invalid x-api-key"}})", "application/json");
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::thread serverThread([&]{ svr.listen_after_bind(); });
    waitUntilServerRunning(svr);

    AiAssistant ai;
    ai.apiKey     = "bad-key";
    ai.apiBaseUrl = "http://127.0.0.1:" + std::to_string(port);
    ai.sendAsync("system prompt", "<mc3/>", "add a box");

    bool finished = pollUntilDone(ai, 5000);
    CHECK(finished, "mock server (error): request completes within the timeout");
    CHECK(ai.hasError(), "mock server (error): a non-200 status is reported as an error");
    CHECK(ai.errorMsg().find("401") != std::string::npos,
          "mock server (error): the error message includes the HTTP status code");

    svr.stop();
    serverThread.join();
}

#endif // MESHCRAFT_HAS_AI

// ─────────────────────────────────────────────────────────────────────────────

int main() {
    testJsonEscape();
    testExtractStopReason();
    testExtractFirstTextValue();
    testExtractXmlPlain();
    testExtractXmlStripsMarkdownFence();
    testExtractXmlWithSurroundingProse();
    testFullPipelineHandlesMarkdownFenceAndProse();
    testRepairXmlInsertsMissingCloseBracket();
    testValidateAndParseInvalidXmlSetsError();
    testValidateAndParseMalformedXmlSetsError();
    testValidateAndParseEmptyDocumentRejected();
    testValidateAndParseAcceptsNonEmptyDocument();
#ifdef MESHCRAFT_HAS_AI
    testMockServerSuccessRoundTrip();
    testMockServerTruncatedResponse();
    testMockServerHttpErrorStatus();
#else
    std::cout << "SKIP: mock HTTP server tests require MESHCRAFT_HAS_AI\n";
#endif

    std::cout << "\n" << (failures == 0 ? "All AI tests passed."
                                        : "FAILURES: " + std::to_string(failures)) << "\n";
    return failures > 0 ? 1 : 0;
}
