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
#include <MeshCraft/Mc3/Mc3Validation.hpp>
#include <MeshCraft/TempFile.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
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

// SYS-W2-04 — redactSecret()/boundedForDisplay(), the pure helpers
// sendAsync() applies to every error-message-construction path.
static void testRedactSecretAndBoundedForDisplay() {
    CHECK(AiAssistant::redactSecret("connection refused", "sk-ant-abc123") ==
          "connection refused",
          "redactSecret: message without the secret is unchanged");
    CHECK(AiAssistant::redactSecret("key was sk-ant-abc123 in the header", "sk-ant-abc123") ==
          "key was [REDACTED] in the header",
          "redactSecret: a single occurrence is replaced with [REDACTED]");
    CHECK(AiAssistant::redactSecret("sk-ant-abc123...sk-ant-abc123", "sk-ant-abc123") ==
          "[REDACTED]...[REDACTED]",
          "redactSecret: every occurrence is replaced, not just the first");
    CHECK(AiAssistant::redactSecret("no secret set here", "") == "no secret set here",
          "redactSecret: an empty secret is a no-op (never redacts everything)");

    CHECK(AiAssistant::boundedForDisplay("short") == "short",
          "boundedForDisplay: text under the limit is returned unchanged");
    std::string big(5000, 'x');
    std::string bounded = AiAssistant::boundedForDisplay(big, 4096);
    CHECK(bounded.size() < big.size(),
          "boundedForDisplay: an oversized body is shortened");
    CHECK(bounded.rfind("... (truncated, 5000 bytes total)") != std::string::npos,
          "boundedForDisplay: the truncation note states the real original size");
    CHECK(AiAssistant::boundedForDisplay(std::string(10, 'y'), 10) == std::string(10, 'y'),
          "boundedForDisplay: text exactly at the limit is not truncated");
}

// STAB-0611/STAB-0635 — uniqueTempPath() must not repeat, even for the same
// prefix/extension in the same process (the bug this replaces was a
// per-process counter that reset to 0 on every launch, so two MeshCraft
// processes could collide on the same tmp filename).
static void testUniqueTempPathNoCollision() {
    std::set<std::string> seen;
    bool anyCollision = false;
    for (int i = 0; i < 1000; ++i) {
        auto p = MeshCraft::uniqueTempPath("stab0635_test", ".tmp");
        if (!seen.insert(p.string()).second) anyCollision = true;
    }
    CHECK(!anyCollision,
          "STAB-0611/STAB-0635: 1000 consecutive uniqueTempPath() calls with "
          "the same prefix/extension never repeat");
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

    // AUD-009(a): a bare "text":"..." field appearing before the real
    // {"type":"text","text":"..."} content block (e.g. a citation or a
    // future block shape that happens to carry its own unrelated "text"
    // key) must NOT be extracted -- only a "text" key genuinely scoped
    // under "type":"text" counts.
    CHECK(AiAssistant::extractFirstTextValue(
              R"({"content":[{"type":"citation","text":"decoy"},)"
              R"({"type":"text","text":"the real answer"}]})")
              == "the real answer",
          "extractFirstTextValue: skips a decoy \"text\" field not scoped under type=text");

    // AUD-009(b): an astral-plane character (outside the Basic
    // Multilingual Plane) is encoded by JSON as a UTF-16 surrogate PAIR
    // (two consecutive \uXXXX escapes). They must be combined into one
    // code point and UTF-8-encoded as 4 bytes -- encoding each half
    // independently (the pre-fix behavior) produces two invalid 3-byte
    // sequences instead. 😀 is U+1F600 (grinning face emoji),
    // UTF-8 F0 9F 98 80.
    std::string jsonWithAstralEscape =
        std::string(R"({"content":[{"type":"text","text":")") + "\\ud83d\\ude00" + R"("}]})";
    std::string expectedEmoji = "\xF0\x9F\x98\x80";
    CHECK(AiAssistant::extractFirstTextValue(jsonWithAstralEscape) == expectedEmoji,
          "extractFirstTextValue: combines a UTF-16 surrogate pair into valid 4-byte UTF-8 (astral emoji)");

    // A lone (unpaired) high surrogate is not silently emitted as invalid
    // UTF-8 -- it's replaced with U+FFFD (EF BF BD), the standard Unicode
    // "invalid sequence" marker.
    std::string jsonWithLoneSurrogate =
        std::string(R"({"content":[{"type":"text","text":"x)") + "\\ud83d" + R"(y"}]})";
    CHECK(AiAssistant::extractFirstTextValue(jsonWithLoneSurrogate) == "x\xEF\xBF\xBDy",
          "extractFirstTextValue: a lone high surrogate becomes U+FFFD, not invalid UTF-8");
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

// ─────────────────────────────────────────────────────────────────────────────
// computeAiChangeSummaryAlg (SYS-W14-08) — the AI-apply preview/diff
// ─────────────────────────────────────────────────────────────────────────────

static std::shared_ptr<Mc3Object> makeAiTestObj(std::string id, std::string name) {
    auto o = std::make_shared<Mc3Object>();
    o->id = std::move(id);
    o->name = std::move(name);
    return o;
}

static void testComputeAiChangeSummaryDetectsAddedAndRemoved() {
    Mc3Document oldDoc, newDoc;
    oldDoc.objects.push_back(makeAiTestObj("a", "A"));
    oldDoc.objects.push_back(makeAiTestObj("b", "B"));
    newDoc.objects.push_back(makeAiTestObj("a", "A"));
    newDoc.objects.push_back(makeAiTestObj("c", "C"));

    auto summary = computeAiChangeSummaryAlg(oldDoc, newDoc);
    CHECK(summary.added.size() == 1 && summary.added[0].id == "c",
          "computeAiChangeSummaryAlg: an id only in the new document is 'added'");
    CHECK(summary.removed.size() == 1 && summary.removed[0].id == "b",
          "computeAiChangeSummaryAlg: an id only in the old document is 'removed'");
    CHECK(summary.modified.empty(),
          "computeAiChangeSummaryAlg: an untouched shared id ('a') is not 'modified'");
}

static void testComputeAiChangeSummaryDetectsModified() {
    Mc3Document oldDoc, newDoc;
    auto oldObj = makeAiTestObj("a", "A");
    oldDoc.objects.push_back(oldObj);
    auto renamed = makeAiTestObj("a", "A-renamed");
    newDoc.objects.push_back(renamed);

    auto summary = computeAiChangeSummaryAlg(oldDoc, newDoc);
    CHECK(summary.modified.size() == 1 && summary.modified[0].id == "a",
          "computeAiChangeSummaryAlg: a name change on a shared id is 'modified'");
    CHECK(summary.added.empty() && summary.removed.empty(),
          "computeAiChangeSummaryAlg: a modified id is not also reported as added/removed");

    // Same check for a transform change, since that's the other high-signal
    // field this intentionally-partial diff covers.
    Mc3Document oldDoc2, newDoc2;
    oldDoc2.objects.push_back(makeAiTestObj("m", "Moved"));
    auto moved = makeAiTestObj("m", "Moved");
    moved->transform.position = {5.f, 0.f, 0.f};
    newDoc2.objects.push_back(moved);
    auto summary2 = computeAiChangeSummaryAlg(oldDoc2, newDoc2);
    CHECK(summary2.modified.size() == 1 && summary2.modified[0].id == "m",
          "computeAiChangeSummaryAlg: a transform.position change on a shared id is 'modified'");
}

static void testComputeAiChangeSummaryIgnoresUnchangedAndUncoveredFields() {
    Mc3Document oldDoc, newDoc;
    auto oldObj = makeAiTestObj("a", "A");
    oldDoc.objects.push_back(oldObj);
    auto sameCore = makeAiTestObj("a", "A");
    // Deliberately differs in a field outside this diff's stated scope
    // (tags aren't one of the compared core fields) -- must NOT be flagged.
    sameCore->tags = {"outside-scope"};
    newDoc.objects.push_back(sameCore);

    auto summary = computeAiChangeSummaryAlg(oldDoc, newDoc);
    CHECK(summary.added.empty() && summary.removed.empty() && summary.modified.empty(),
          "computeAiChangeSummaryAlg: identical core fields report no change, even if an "
          "out-of-scope field (tags) differs -- matches this diff's documented partial scope");
}

static void testComputeAiChangeSummaryWalksNestedChildren() {
    Mc3Document oldDoc, newDoc;
    auto oldParent = makeAiTestObj("p", "Parent");
    auto oldChild  = makeAiTestObj("c", "Child");
    oldParent->children.push_back(oldChild);
    oldDoc.objects.push_back(oldParent);

    auto newParent = makeAiTestObj("p", "Parent");
    auto newChild  = makeAiTestObj("c", "Child-renamed");
    newParent->children.push_back(newChild);
    newDoc.objects.push_back(newParent);

    auto summary = computeAiChangeSummaryAlg(oldDoc, newDoc);
    CHECK(summary.modified.size() == 1 && summary.modified[0].id == "c",
          "computeAiChangeSummaryAlg: a nested child's change is detected, not just top-level objects");
}

static void testComputeAiChangeSummaryDuplicateIdFirstMatchWins() {
    Mc3Document oldDoc, newDoc;
    oldDoc.objects.push_back(makeAiTestObj("dup", "First"));
    oldDoc.objects.push_back(makeAiTestObj("dup", "Second"));
    newDoc.objects.push_back(makeAiTestObj("dup", "First"));
    newDoc.objects.push_back(makeAiTestObj("dup", "Second-changed"));

    auto summary = computeAiChangeSummaryAlg(oldDoc, newDoc);
    // Matches Editor::ObjectIndex's own "first match in document order"
    // semantics for duplicate ids: the FIRST "dup" (unchanged "First") wins
    // the comparison on both sides, so this is reported as unmodified.
    CHECK(summary.modified.empty(),
          "computeAiChangeSummaryAlg: duplicate ids resolve to the first occurrence "
          "in document order on both sides, matching ObjectIndex's convention");
}

static void testComputeAiChangeSummaryCyclicChildrenThrows() {
    Mc3Document cyclicDoc, otherDoc;
    auto x = makeAiTestObj("x", "X");
    auto y = makeAiTestObj("y", "Y");
    x->children.push_back(y);
    y->children.push_back(x); // 2-cycle
    cyclicDoc.objects.push_back(x);
    otherDoc.objects.push_back(makeAiTestObj("z", "Z"));

    bool threw = false;
    try {
        (void)computeAiChangeSummaryAlg(cyclicDoc, otherDoc);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw, "computeAiChangeSummaryAlg: a cyclic children graph throws instead of crashing");
}

// STAB-0410 — a response with only <definitions> and no top-level <objects>
// is a legitimate AI result (e.g. "add a reusable crate definition to the
// library") and must not be rejected by the same empty-document guard that
// exists to reject a truly empty response.
static void testValidateAndParseAcceptsDefinitionsOnlyDocument() {
    auto result = validateAndParseAiResponseAlg(
        "<mc3 version=\"0.3\">"
        "<definitions><definition id=\"crate\"><box size=\"1 1 1\"/></definition></definitions>"
        "</mc3>");
    CHECK(result.doc.has_value(),
          "STAB-0410: a definitions-only response (no <objects>) is accepted, not rejected "
          "as an empty document");
    if (result.doc) {
        CHECK(result.doc->objects.empty(),
              "STAB-0410: the accepted document indeed has no objects");
        CHECK(!result.doc->definitions.empty(),
              "STAB-0410: ...but does have the definition that makes it non-empty");
    }
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

// AUD-010: parseXmlAlg (the AI-response entry point) already calls
// Mc3Document::loadFromString with Mc3LoadPolicy::untrusted(), which sets
// allowIncludes=false -- but that was never actually exercised through the
// AI-response pipeline by any test (only the underlying include-resolution
// machinery itself, via mc3/test/roundtrip_test.cpp, and Mc3LoadPolicy
// directly, via mc3/test/load_policy_test.cpp). A regression that
// accidentally routed AI responses through Mc3LoadPolicy::trusted() instead
// would have passed CI undetected. Proves it with a REAL file whose content
// would be observably merged if <include> were honored -- the absence of
// that content is the actual proof, not merely "no exception was thrown".
static void testAiResponseIncludeIsIgnoredNotResolved() {
    auto secretPath = uniqueTempPath("mc3_ai_test_secret", ".mc3.xml");
    {
        std::ofstream f(secretPath);
        f << R"(<mc3 version="0.3"><definitions>)"
             R"(<definition id="leaked_from_ai_include"><box name="b"/></definition>)"
             R"(</definitions></mc3>)";
    }

    std::string maliciousResponse =
        "<mc3 version=\"0.3\">"
        "<include file=\"" + secretPath.string() + "\"/>"
        "<objects><box name=\"visible\"/></objects>"
        "</mc3>";

    auto result = validateAndParseAiResponseAlg(maliciousResponse);
    CHECK(result.doc.has_value(),
          "AUD-010: an AI response containing <include> still parses successfully "
          "(the include is silently ignored, not treated as a fatal error)");
    if (result.doc) {
        CHECK(result.doc->definitions.count("leaked_from_ai_include") == 0,
              "AUD-010: <include> in an AI response is NOT resolved -- the target file's "
              "content (a distinctive definition id) is absent from the parsed document");
        bool hasVisible = false;
        for (const auto& o : result.doc->objects) if (o && o->name == "visible") hasVisible = true;
        CHECK(hasVisible, "AUD-010: the rest of the malicious response still parses normally");
    }

    // Same, for a path-traversal-shaped target that doesn't even exist --
    // must still be silently ignored, since allowIncludes=false skips
    // <include> processing unconditionally, regardless of what the path
    // looks like (no path-shape-dependent special case to bypass).
    auto traversalResult = validateAndParseAiResponseAlg(
        "<mc3 version=\"0.3\">"
        "<include file=\"../../../../etc/passwd\"/>"
        "<objects><box name=\"visible2\"/></objects>"
        "</mc3>");
    CHECK(traversalResult.doc.has_value(),
          "AUD-010: a path-traversal-shaped <include> target is ignored, not treated as an "
          "error or attempted, exactly like any other <include> in an AI response");

    std::filesystem::remove(secretPath);
}

// STAB-0392 — a real bug found while writing this test: parseXmlAlg wrote its
// scratch file, then only removed it *after* a successful
// Mc3Document::loadFromFile() call — malformed XML (a common real-world AI
// response, exercised right above by testValidateAndParseMalformedXmlSetsError)
// made loadFromFile() throw before the removal line ran, leaking a temp file
// on every single malformed response. Fixed with a try/catch that always
// removes the scratch file. This test counts "mc_ai_parse_*" files in the
// system temp dir before/after several malformed-response validation calls.
static void testMalformedResponseDoesNotLeakTempFiles() {
    namespace fs = std::filesystem;
    auto countScratchFiles = [] {
        int n = 0;
        std::error_code ec;
        auto tempDir = fs::temp_directory_path(ec);
        if (ec) return 0;
        for (const auto& entry : fs::directory_iterator(tempDir, ec)) {
            if (entry.path().filename().string().rfind("mc_ai_parse_", 0) == 0)
                ++n;
        }
        return n;
    };

    int before = countScratchFiles();
    for (int i = 0; i < 20; ++i) {
        // Has "<mc3" so it passes the root-marker check, but isn't valid XML
        // (same shape as testValidateAndParseMalformedXmlSetsError above) —
        // this is exactly the path that used to leak parseXmlAlg's temp file.
        auto result = validateAndParseAiResponseAlg("<mc3 version=\"0.3\"><objects>");
        (void)result;
    }
    int after = countScratchFiles();

    CHECK(after == before,
          "STAB-0392: 20 malformed AI responses leave no leftover "
          "mc_ai_parse_* temp files behind");
}

// ─────────────────────────────────────────────────────────────────────────────
// STAB-0391 — AI response XSD validation (validateXmlAgainstXsdAlg, embedded
// mc3.xsd). Also exercised end-to-end through validateAndParseAiResponseAlg,
// which is what MeshCraftApplication_UiAi.cpp actually calls.
// ─────────────────────────────────────────────────────────────────────────────

static void testValidateXmlAgainstXsdAcceptsValidDocument() {
    // Schema-conformant: box's only required-by-convention attribute is
    // `id` (added to objectAttrs this session — Mc3XmlWriter/Parser already
    // read/write it, but mc3.xsd never declared it, a real pre-existing gap
    // found while wiring up this test), plus a valid vec3Type `size`.
    auto err = validateXmlAgainstXsdAlg(
        "<mc3 version=\"0.3\"><objects><box id=\"b1\" size=\"1 1 1\"/></objects></mc3>");
    CHECK(!err.has_value(), "STAB-0391: a schema-conformant document passes XSD validation");
}

static void testValidateAndParseAiResponsePipelineAcceptsValidXsd() {
    auto result = validateAndParseAiResponseAlg(
        "<mc3 version=\"0.3\"><objects><box id=\"b1\" size=\"1 1 1\"/></objects></mc3>");
    CHECK(result.doc.has_value(),
          "STAB-0391: the full pipeline accepts a schema-conformant AI response");
}

// Regression test for a follow-up audit (post-STAB-0391): mc3.xsd was
// missing declarations for several attributes/elements that
// Mc3XmlWriter/Mc3XmlParser actually read/write — object `layer`, instance
// `material_override`/`variants`, and per-object `<state>` children chief
// among them (see plan.md). Each would have made a perfectly valid,
// round-tripping AI response get wrongly rejected by this exact pipeline.
// Fixed in mc3.xsd; this test exercises all three constructs at once so a
// future regression here fails loudly.
static void testValidateAndParseAiResponseAcceptsPreviouslyUndeclaredConstructs() {
    auto result = validateAndParseAiResponseAlg(
        "<mc3 version=\"0.3\">"
        "<materials><material id=\"stone\"/></materials>"
        "<definitions><definition id=\"crate\"><box size=\"1 1 1\"/></definition></definitions>"
        "<objects>"
        "<box id=\"b1\" size=\"1 1 1\" layer=\"Foreground\">"
        "<state id=\"on\" visible=\"true\"/>"
        "</box>"
        "<instance id=\"i1\" definition=\"crate\" material_override=\"stone\" variants=\"a b\"/>"
        "</objects></mc3>");
    CHECK(result.doc.has_value(),
          "XSD audit regression: layer/state/material_override/variants no longer false-reject");
}

// ─────────────────────────────────────────────────────────────────────────────
// SYS-W1-01 — validateAndParseAiResponseAlg(rawResponse, validation): the
// AI-apply integration point. Proves the diagnostic surface itself (mirrors
// mc3_validation_test.cpp's approach), for the single least-trusted content
// source this application parses.
// ─────────────────────────────────────────────────────────────────────────────

static void testValidateAndParseAiResponseValidationOverload() {
    // A hostile-but-recoverable value: the parser clamps it (doesn't
    // reject), so the document still parses, but a warning entry must
    // appear naming the field.
    Mc3Validation validation;
    auto result = validateAndParseAiResponseAlg(
        "<mc3 version=\"0.3\"><objects>"
        "<sphere id=\"huge\" name=\"huge\" radius=\"1\" segments=\"100000000\"/>"
        "</objects></mc3>", validation);
    CHECK(result.doc.has_value(),
          "SYS-W1-01: a hostile-but-recoverable AI response still parses to a usable document");
    CHECK(validation.hasWarnings(),
          "SYS-W1-01: the validation-capturing overload records the segments clamp as a warning");

    // A hard rejection (no <mc3> root) must ALSO record an error entry, not
    // just set errorMessage -- this rejection has no Mc3XmlParser equivalent
    // (it never reaches the parser at all), so it's recorded directly here.
    Mc3Validation rejectValidation;
    auto rejected = validateAndParseAiResponseAlg("not xml here, sorry", rejectValidation);
    CHECK(!rejected.doc.has_value(), "SYS-W1-01: still rejects a non-XML response");
    CHECK(rejectValidation.hasErrors(),
          "SYS-W1-01: the missing-<mc3>-root rejection is ALSO recorded as a validation error "
          "entry, not just returned via errorMessage");

    // The empty-document rejection (STAB-0376) must be recorded too.
    Mc3Validation emptyValidation;
    auto empty = validateAndParseAiResponseAlg("<mc3 version=\"0.3\"></mc3>", emptyValidation);
    CHECK(!empty.doc.has_value(), "SYS-W1-01: still rejects an empty document");
    CHECK(emptyValidation.hasErrors(),
          "SYS-W1-01: the empty-document rejection is recorded as a validation error entry too");

    // A clean, unremarkable response must produce zero findings.
    Mc3Validation cleanValidation;
    auto clean = validateAndParseAiResponseAlg(
        "<mc3 version=\"0.3\"><objects><box id=\"b1\"/></objects></mc3>", cleanValidation);
    CHECK(clean.doc.has_value(), "SYS-W1-01: a clean response still parses normally");
    CHECK(cleanValidation.empty(),
          "SYS-W1-01: a clean response produces no validation findings at all");
}

#ifdef MESHCRAFT_HAS_LIBXML2
// These two only hold when libxml2 is actually compiled in — without it,
// validateXmlAgainstXsdAlg is a no-op that never rejects (see its doc
// comment), by design: schema validation degrades gracefully rather than
// blocking "Apply to Scene" on builds without libxml2.

static void testValidateXmlAgainstXsdRejectsInvalidDocument() {
    // `role` is an enumeration of just "cutter" (mc3.xsd) — "bogus" is not
    // a member. Well-formed XML, structurally parseable by Mc3Document
    // (Mc3XmlParser just compares role=="cutter" as a plain string), but a
    // genuine schema violation.
    auto err = validateXmlAgainstXsdAlg(
        "<mc3 version=\"0.3\"><objects><box id=\"b1\" role=\"bogus\"/></objects></mc3>");
    CHECK(err.has_value(), "STAB-0391: a schema-violating document is rejected");
    if (err)
        CHECK(!err->empty(), "STAB-0391: the rejection includes a non-empty error message");
}

static void testValidateAndParseAiResponsePipelineRejectsInvalidXsd() {
    auto result = validateAndParseAiResponseAlg(
        "<mc3 version=\"0.3\"><objects><box id=\"b1\" role=\"bogus\"/></objects></mc3>");
    CHECK(!result.doc.has_value(),
          "STAB-0391: the full pipeline rejects an AI response that violates mc3.xsd");
    CHECK(result.errorMessage.find("mc3.xsd") != std::string::npos,
          "STAB-0391: the rejection error names the schema");
}
#endif // MESHCRAFT_HAS_LIBXML2

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

// SYS-W2-04 — a pathological/misconfigured server (e.g. apiBaseUrl pointed
// somewhere it shouldn't be) returning a huge error body must not produce a
// proportionally huge errorMsg(); AiAssistant::boundedForDisplay() must
// actually be wired into the real sendAsync() error path, not just proven
// correct in isolation above.
static void testMockServerOversizedErrorBodyIsBounded() {
    const std::string hugeBody(20000, 'z');
    httplib::Server svr;
    svr.Post("/v1/messages", [&](const httplib::Request&, httplib::Response& res) {
        res.status = 500;
        res.set_content(hugeBody, "text/plain");
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::thread serverThread([&]{ svr.listen_after_bind(); });
    waitUntilServerRunning(svr);

    AiAssistant ai;
    ai.apiKey     = "test-key";
    ai.apiBaseUrl = "http://127.0.0.1:" + std::to_string(port);
    ai.sendAsync("system prompt", "<mc3/>", "add a box");

    bool finished = pollUntilDone(ai, 5000);
    CHECK(finished, "oversized error body: request completes within the timeout");
    CHECK(ai.hasError(), "oversized error body: a 500 status is reported as an error");
    CHECK(ai.errorMsg().size() < hugeBody.size(),
          "oversized error body: errorMsg() is bounded well below the 20000-byte body "
          "sendAsync() actually received (got " + std::to_string(ai.errorMsg().size()) +
          " bytes)");
    CHECK(ai.errorMsg().find("truncated") != std::string::npos,
          "oversized error body: errorMsg() states that truncation happened");

    svr.stop();
    serverThread.join();
}

// SYS-W2-05 — the response size cap must actually abort the network READ
// itself (via the streaming content_receiver), not just truncate an
// already-fully-buffered body afterward (that's SYS-W2-04, proven above).
// Uses a small maxResponseBytes override so the test doesn't need to
// actually transfer megabytes to prove the behavior.
static void testMockServerResponseExceedingCapIsAborted() {
    const std::string oversized(2000, 'q'); // > the 500-byte cap below
    httplib::Server svr;
    svr.Post("/v1/messages", [&](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        res.set_content(oversized, "application/json");
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::thread serverThread([&]{ svr.listen_after_bind(); });
    waitUntilServerRunning(svr);

    AiAssistant ai;
    ai.apiKey          = "test-key";
    ai.apiBaseUrl      = "http://127.0.0.1:" + std::to_string(port);
    ai.maxResponseBytes = 500;
    ai.sendAsync("system prompt", "<mc3/>", "add a box");

    bool finished = pollUntilDone(ai, 5000);
    CHECK(finished, "response cap: request completes within the timeout, not a hang");
    CHECK(ai.hasError(),
          "response cap: a response exceeding maxResponseBytes is reported as an error, "
          "not silently accepted");
    CHECK(ai.errorMsg().find("500") != std::string::npos,
          "response cap: the error message names the configured byte cap");

    svr.stop();
    serverThread.join();
}

// SYS-W2-05 — a response comfortably under the cap must still work exactly
// as before (the streaming rewrite must not have broken the success path).
static void testMockServerResponseUnderCapStillSucceeds() {
    httplib::Server svr;
    svr.Post("/v1/messages", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(
            R"({"content":[{"type":"text","text":"<mc3 version=\"0.3\"/>"}],"stop_reason":"end_turn"})",
            "application/json");
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::thread serverThread([&]{ svr.listen_after_bind(); });
    waitUntilServerRunning(svr);

    AiAssistant ai;
    ai.apiKey          = "test-key";
    ai.apiBaseUrl      = "http://127.0.0.1:" + std::to_string(port);
    ai.maxResponseBytes = 500; // small but well above this tiny fixture's real size
    ai.sendAsync("system prompt", "<mc3/>", "add a box");

    bool finished = pollUntilDone(ai, 5000);
    CHECK(finished, "response under cap: request completes within the timeout");
    CHECK(!ai.hasError(), "response under cap: no error reported");
    CHECK(ai.result().find("<mc3") != std::string::npos,
          "response under cap: the response text is still extracted correctly");

    svr.stop();
    serverThread.join();
}

// STAB-0381 — the Model field is genuinely user-configurable: whatever value
// is set on AiAssistant::model ends up verbatim in the outgoing request body,
// not just editable in the UI with no effect on the wire.
static void testModelNameSentInRequestBody() {
    std::string capturedBody;
    httplib::Server svr;
    svr.Post("/v1/messages", [&](const httplib::Request& req, httplib::Response& res) {
        capturedBody = req.body;
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
    ai.model      = "claude-sonnet-5-custom-test-model";
    ai.sendAsync("system prompt", "<mc3/>", "add a box");

    bool finished = pollUntilDone(ai, 5000);
    CHECK(finished, "model name test: request completes within the timeout");
    CHECK(capturedBody.find("\"model\":\"claude-sonnet-5-custom-test-model\"") != std::string::npos,
          "STAB-0381: the AiAssistant::model value is sent verbatim as the JSON \"model\" field, "
          "confirming the UI's Model field genuinely controls what is sent to the API");

    svr.stop();
    serverThread.join();
}

// STAB-0382 — a network error (nothing listening on the target port) must
// surface as hasError()/errorMsg(), not a crash or an indefinite hang.
static void testConnectionRefusedProducesUserVisibleError() {
    AiAssistant ai;
    ai.apiKey            = "test-key";
    // Port 1 is a reserved, essentially-never-listened-on port — connecting
    // to it fails immediately with "connection refused" on any CI/dev box.
    ai.apiBaseUrl        = "http://127.0.0.1:1";
    ai.connectTimeoutSec = 3;
    ai.readTimeoutSec    = 3;
    ai.writeTimeoutSec   = 3;

    ai.sendAsync("system prompt", "<mc3/>", "add a box");
    bool finished = pollUntilDone(ai, 5000);

    CHECK(finished, "STAB-0382: a connection-refused request completes (doesn't hang)");
    CHECK(ai.hasError(), "STAB-0382: a network error is reported via hasError()");
    CHECK(!ai.errorMsg().empty(),
          "STAB-0382: a non-empty, user-visible error message is set via errorMsg()");
}

// STAB-0400 — a 200 response whose body isn't valid JSON at all (e.g. an API
// gateway returning an HTML error page or truncated body) must not crash the
// substring-search-based JSON extraction; it should degrade to an empty
// result that the downstream validation pipeline then rejects cleanly.
static void testMalformedJsonResponseHandledGracefully() {
    httplib::Server svr;
    svr.Post("/v1/messages", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{not valid json at all <<< ]]", "text/plain");
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::thread serverThread([&]{ svr.listen_after_bind(); });
    waitUntilServerRunning(svr);

    AiAssistant ai;
    ai.apiKey     = "test-key";
    ai.apiBaseUrl = "http://127.0.0.1:" + std::to_string(port);
    ai.sendAsync("system prompt", "<mc3/>", "add a box");

    bool finished = pollUntilDone(ai, 5000);
    CHECK(finished, "STAB-0400: malformed-JSON response completes (doesn't hang or crash)");
    // extractFirstTextValue() finds no "text":"..." key in non-JSON content and
    // returns empty — sendAsync() already has an explicit guard for exactly this
    // ("Empty response from API: ..."), so it correctly surfaces as an
    // AiAssistant-level error rather than silently continuing with an empty result.
    CHECK(ai.hasError(),
          "STAB-0400: a 200 response with no extractable text is reported as an error "
          "(existing 'Empty response from API' guard), not silently swallowed");
    CHECK(!ai.errorMsg().empty(),
          "STAB-0400: a non-empty, user-visible error message is set");
    CHECK(ai.result().empty(),
          "STAB-0400: no <text> value could be extracted from non-JSON content");

    svr.stop();
    serverThread.join();
}

// STAB-0403 — sendAsync() itself has no internal re-entrancy guard (by
// design: the UI layer is the single caller, and MeshCraftApplication_UiAi.cpp
// disables the Send button whenever isInFlight() is true, and isInFlight()
// flips true synchronously the instant sendAsync() returns — before the next
// frame can render — so a real double-click can never reach sendAsync()
// twice). This test exercises the API layer directly, confirming that if it
// were ever called twice back-to-back anyway, the second call's request
// simply supersedes the first (the first's detached thread keeps running
// harmlessly to completion but its result is unreachable) rather than
// crashing or corrupting state.
static void testBackToBackSendAsyncCallsDoNotCrash() {
    // The first request's connection is deliberately held open (blocked on a
    // condition_variable) so it can never race the second — the server tells
    // the two requests apart by their task-prompt text, embedded verbatim in
    // the JSON body ("Task: first request" vs "Task: second request").
    std::mutex              releaseMutex;
    std::condition_variable releaseCv;
    bool                    release = false;

    httplib::Server svr;
    svr.Post("/v1/messages", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.body.find("first request") != std::string::npos) {
            std::unique_lock<std::mutex> lk(releaseMutex);
            releaseCv.wait(lk, [&] { return release; });
            res.set_content(
                R"({"content":[{"type":"text","text":"<mc3 version=\"0.3\"><objects><box id=\"b1\"/></objects></mc3>"}],"stop_reason":"end_turn"})",
                "application/json");
        } else {
            res.set_content(
                R"({"content":[{"type":"text","text":"<mc3 version=\"0.3\"><objects><box id=\"b2\"/></objects></mc3>"}],"stop_reason":"end_turn"})",
                "application/json");
        }
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::thread serverThread([&]{ svr.listen_after_bind(); });
    waitUntilServerRunning(svr);

    AiAssistant ai;
    ai.apiKey     = "test-key";
    ai.apiBaseUrl = "http://127.0.0.1:" + std::to_string(port);
    ai.sendAsync("system prompt", "<mc3/>", "first request");   // blocks server-side, discarded
    ai.sendAsync("system prompt", "<mc3/>", "second request");  // supersedes the first

    bool finished = pollUntilDone(ai, 5000);
    CHECK(finished, "STAB-0403: the second sendAsync() call completes normally");
    CHECK(!ai.hasError(), "STAB-0403: no error from calling sendAsync() twice back-to-back");
    CHECK(ai.result().find("b2") != std::string::npos,
          "STAB-0403: polling only ever observes the second (superseding) request's result");

    // Release the first (blocked, now-orphaned) request so its detached
    // thread can finish and the server can shut down cleanly.
    {
        std::lock_guard<std::mutex> lk(releaseMutex);
        release = true;
    }
    releaseCv.notify_all();
    svr.stop();
    serverThread.join();
}

// STAB-0383 / STAB-0631 — a server that accepts the connection but never
// responds must not hang sendAsync() forever: the configured read timeout
// has to fire and report an error.
static void testNetworkTimeoutPreventsIndefiniteHang() {
    std::mutex              releaseMutex;
    std::condition_variable releaseCv;
    bool                    release = false;

    httplib::Server svr;
    svr.Post("/v1/messages", [&](const httplib::Request&, httplib::Response&) {
        // Block until the test releases us — simulates a server that hangs
        // indefinitely rather than one that merely responds slowly.
        std::unique_lock<std::mutex> lk(releaseMutex);
        releaseCv.wait(lk, [&] { return release; });
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::thread serverThread([&]{ svr.listen_after_bind(); });
    waitUntilServerRunning(svr);

    AiAssistant ai;
    ai.apiKey            = "test-key";
    ai.apiBaseUrl        = "http://127.0.0.1:" + std::to_string(port);
    ai.connectTimeoutSec = 1;
    ai.readTimeoutSec    = 1; // far shorter than the server's indefinite hang
    ai.writeTimeoutSec   = 1;

    auto start = std::chrono::steady_clock::now();
    ai.sendAsync("system prompt", "<mc3/>", "add a box");
    bool finished = pollUntilDone(ai, 4000);
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    CHECK(finished,
          "STAB-0383/STAB-0631: sendAsync() returns instead of hanging "
          "forever when the server never responds");
    CHECK(ai.hasError(),
          "STAB-0383/STAB-0631: a timed-out request is reported as an error");
    CHECK(elapsedMs < 4000,
          "STAB-0383/STAB-0631: the timeout fires close to the configured "
          "readTimeoutSec (1s), not after an indefinite wait");

    {
        std::lock_guard<std::mutex> lk(releaseMutex);
        release = true;
    }
    releaseCv.notify_all();
    svr.stop();
    serverThread.join();
}

// AUD-014: sendAsync()'s worker thread is detached, so nothing previously
// stopped the process from exiting (and beginning static/OpenSSL teardown)
// while that thread was still executing httplib/OpenSSL code.
// AiAssistant::waitForAllInFlight() is the fix -- called once by main.cpp
// right before the process would otherwise exit. Exercises both halves of
// its contract directly against a real in-flight worker thread (not just
// the happy path where nothing is running): (1) it is genuinely BOUNDED --
// while a request is still blocked server-side, a short-timeout call
// returns false rather than hanging (this is the property that keeps it
// from reintroducing the exact indefinite-hang bug STAB-0387/0388 removed
// from reset()); (2) it genuinely WAITS -- once the server responds, a
// longer-timeout call observes the worker actually finish and returns true.
static void testWaitForAllInFlightBoundedThenCompletes() {
    std::mutex              releaseMutex;
    std::condition_variable releaseCv;
    bool                    release = false;

    httplib::Server svr;
    svr.Post("/v1/messages", [&](const httplib::Request&, httplib::Response& res) {
        std::unique_lock<std::mutex> lk(releaseMutex);
        releaseCv.wait(lk, [&] { return release; });
        res.set_content(
            R"({"content":[{"type":"text","text":"<mc3 version=\"0.3\"/>"}],"stop_reason":"end_turn"})",
            "application/json");
    });
    int port = svr.bind_to_any_port("127.0.0.1");
    std::thread serverThread([&]{ svr.listen_after_bind(); });
    waitUntilServerRunning(svr);

    AiAssistant ai;
    ai.apiKey            = "test-key";
    ai.apiBaseUrl        = "http://127.0.0.1:" + std::to_string(port);
    // Deliberately far longer than this test's own wait windows below, so
    // the client's own read timeout can't be what finishes the request --
    // only releasing the mock server can.
    ai.connectTimeoutSec = 10;
    ai.readTimeoutSec    = 10;
    ai.writeTimeoutSec   = 10;

    ai.sendAsync("system prompt", "<mc3/>", "add a box");

    bool finishedTooEarly = AiAssistant::waitForAllInFlight(std::chrono::milliseconds(150));
    CHECK(!finishedTooEarly,
          "AUD-014: waitForAllInFlight() returns false (bounded, not hung) "
          "while the worker is still genuinely blocked server-side");

    {
        std::lock_guard<std::mutex> lk(releaseMutex);
        release = true;
    }
    releaseCv.notify_all();

    bool finishedInTime = AiAssistant::waitForAllInFlight(std::chrono::milliseconds(4000));
    CHECK(finishedInTime,
          "AUD-014: waitForAllInFlight() returns true once the released "
          "worker actually finishes, within its timeout");

    // The worker's result should already be fully visible -- poll should
    // need zero further waiting since waitForAllInFlight() only returned
    // true after the AiWorkerScopeGuard destructor ran, which is strictly
    // after pending->done.store() in sendAsync()'s lambda.
    bool polled = pollUntilDone(ai, 100);
    CHECK(polled && !ai.hasError(),
          "AUD-014: after waitForAllInFlight() returns true, the result is "
          "already fully published (no race between the wait and the data)");

    svr.stop();
    serverThread.join();
}

#endif // MESHCRAFT_HAS_AI

// ─────────────────────────────────────────────────────────────────────────────

int main() {
    testUniqueTempPathNoCollision();
    testJsonEscape();
    testRedactSecretAndBoundedForDisplay();
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
    testComputeAiChangeSummaryDetectsAddedAndRemoved();
    testComputeAiChangeSummaryDetectsModified();
    testComputeAiChangeSummaryIgnoresUnchangedAndUncoveredFields();
    testComputeAiChangeSummaryWalksNestedChildren();
    testComputeAiChangeSummaryDuplicateIdFirstMatchWins();
    testComputeAiChangeSummaryCyclicChildrenThrows();
    testValidateAndParseAcceptsNonEmptyDocument();
    testAiResponseIncludeIsIgnoredNotResolved();
    testValidateAndParseAcceptsDefinitionsOnlyDocument();
    testMalformedResponseDoesNotLeakTempFiles();
    testValidateXmlAgainstXsdAcceptsValidDocument();
    testValidateAndParseAiResponsePipelineAcceptsValidXsd();
    testValidateAndParseAiResponseAcceptsPreviouslyUndeclaredConstructs();
    testValidateAndParseAiResponseValidationOverload();
#ifdef MESHCRAFT_HAS_LIBXML2
    testValidateXmlAgainstXsdRejectsInvalidDocument();
    testValidateAndParseAiResponsePipelineRejectsInvalidXsd();
#else
    std::cout << "SKIP: XSD-rejection tests require MESHCRAFT_HAS_LIBXML2\n";
#endif
#ifdef MESHCRAFT_HAS_AI
    testMockServerSuccessRoundTrip();
    testMockServerTruncatedResponse();
    testMockServerHttpErrorStatus();
    testMockServerOversizedErrorBodyIsBounded();
    testMockServerResponseExceedingCapIsAborted();
    testMockServerResponseUnderCapStillSucceeds();
    testModelNameSentInRequestBody();
    testConnectionRefusedProducesUserVisibleError();
    testMalformedJsonResponseHandledGracefully();
    testBackToBackSendAsyncCallsDoNotCrash();
    testNetworkTimeoutPreventsIndefiniteHang();
    testWaitForAllInFlightBoundedThenCompletes();
#else
    std::cout << "SKIP: mock HTTP server tests require MESHCRAFT_HAS_AI\n";
#endif

    std::cout << "\n" << (failures == 0 ? "All AI tests passed."
                                        : "FAILURES: " + std::to_string(failures)) << "\n";
    return failures > 0 ? 1 : 0;
}
