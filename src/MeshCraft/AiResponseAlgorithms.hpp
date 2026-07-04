#pragma once
// Pure AI-response-parsing algorithms — no CNA / ImGui / SDL / OpenGL /
// network dependencies. Included by MeshCraftApplication_UiAi.cpp (the real
// AI panel, which reads/writes ImGui-coupled state around these calls) and
// ai_test.cpp (headless).

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/TempFile.hpp>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#ifdef MESHCRAFT_HAS_LIBXML2
#include <MeshCraft/Mc3XsdEmbed.hpp>
#include <libxml/parser.h>
#include <libxml/xmlschemastypes.h>
#endif

namespace MeshCraft {

// Extract the XML payload from a raw AI response: finds the first "<?xml"
// or, failing that, the first "<mc3", then also trims any trailing content
// after the matching "</mc3>" close tag. Real AI responses routinely arrive
// wrapped in a markdown code fence (```xml ... ```) or followed by chatty
// prose ("Hope that helps!") despite the system prompt's explicit "no
// prose, no markdown fences" instruction — models don't always follow that
// perfectly. Mc3Document::loadFromFile()/tinyxml2 do NOT tolerate trailing
// bytes after the root element closes (confirmed empirically in ai_test.cpp
// — this was a real gap: without this trim, a fenced or chatty response
// failed to parse even though it contained perfectly valid XML). If neither
// start marker is present, returns the input unchanged (the caller's
// subsequent parse then fails with a useful "no <mc3> root" error rather
// than this function silently returning something misleading); if a start
// marker is present but no "</mc3>" is found (e.g. a self-closing
// "<mc3 .../>" root), everything from the start marker onward is kept as-is.
inline std::string extractXmlAlg(const std::string& s)
{
    auto pos = s.find("<?xml");
    if (pos == std::string::npos) pos = s.find("<mc3");
    if (pos == std::string::npos) return s;
    std::string tail = s.substr(pos);
    const std::string closeTag = "</mc3>";
    auto closePos = tail.find(closeTag);
    if (closePos != std::string::npos)
        return tail.substr(0, closePos + closeTag.size());
    return tail;
}

// Fix a common AI XML mistake: an opening tag not closed with '>' before the
// next element starts. Scans char-by-char; when inside a tag and a bare '<'
// appears, injects the missing '>' first.
inline std::string repairXmlAlg(const std::string& xml)
{
    std::string out;
    out.reserve(xml.size() + 32);
    bool inTag  = false;
    bool inVal  = false;
    char quote  = 0;
    for (size_t i = 0; i < xml.size(); ++i) {
        char c = xml[i];
        if (inTag) {
            if (inVal) {
                out += c;
                if (c == quote) inVal = false;
            } else if (c == '"' || c == '\'') {
                inVal = true; quote = c; out += c;
            } else if (c == '>') {
                inTag = false; out += c;
            } else if (c == '<') {
                // Missing '>' — inject it, then start the new tag
                out += '>';
                inTag = false;
                out += c;
                inTag = true; inVal = false;
            } else {
                out += c;
            }
        } else {
            if (c == '<') inTag = true;
            out += c;
        }
    }
    return out;
}

// Parse an xml string into a document; throws on malformed XML or a missing
// <mc3> root (via Mc3Document::loadFromFile, round-tripped through a temp
// file since the parser is file-based).
inline Mc3::Mc3Document parseXmlAlg(const std::string& xml)
{
    namespace fs = std::filesystem;
    auto tmp = uniqueTempPath("mc_ai_parse", ".mc3.xml");
    { std::ofstream f(tmp); f << xml; }
    auto doc = Mc3::Mc3Document::loadFromFile(tmp);
    std::error_code ec;
    fs::remove(tmp, ec);
    return doc;
}

// A parsed AI response that has neither objects nor definitions would
// silently wipe the current scene if applied — reject it instead.
inline bool isEmptyMc3DocumentAlg(const Mc3::Mc3Document& doc)
{
    return doc.objects.empty() && doc.definitions.empty();
}

#ifdef MESHCRAFT_HAS_LIBXML2
namespace detail {
inline void xsdErrorCollectorAlg(void* ctx, const char* msg, ...)
{
    auto* out = static_cast<std::string*>(ctx);
    char buf[512];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buf, sizeof(buf), msg, args);
    va_end(args);
    *out += buf;
}
} // namespace detail
#endif

// Validate `xml` (already extracted/repaired, structurally well-formed XML)
// against the embedded mc3.xsd schema (STAB-0391) — generated at CMake
// configure time from mc3/mc3.xsd (see Mc3XsdEmbed.hpp.in), so this never
// depends on a runtime file path or the working directory. Returns the
// first schema-validation error message if `xml` doesn't conform, or
// std::nullopt if it does. When libxml2 isn't available at compile time
// (MESHCRAFT_HAS_LIBXML2 undefined), this is a no-op that always returns
// std::nullopt — structural (tinyxml2) parsing already happened upstream in
// parseXmlAlg, so schema validation degrades gracefully rather than
// blocking "Apply to Scene" on builds without libxml2.
inline std::optional<std::string> validateXmlAgainstXsdAlg(const std::string& xml)
{
#ifdef MESHCRAFT_HAS_LIBXML2
    xmlSchemaParserCtxtPtr xsdCtx =
        xmlSchemaNewMemParserCtxt(kMc3XsdContent, static_cast<int>(std::strlen(kMc3XsdContent)));
    if (!xsdCtx) return std::string("Internal error: could not create XSD parser context");
    xmlSchemaPtr schema = xmlSchemaParse(xsdCtx);
    xmlSchemaFreeParserCtxt(xsdCtx);
    if (!schema) return std::string("Internal error: embedded mc3.xsd failed to parse");

    xmlDocPtr doc = xmlReadMemory(xml.c_str(), static_cast<int>(xml.size()), "ai_response.xml", nullptr, 0);
    if (!doc) {
        xmlSchemaFree(schema);
        return std::string("XML is not well-formed");
    }

    xmlSchemaValidCtxtPtr validCtx = xmlSchemaNewValidCtxt(schema);
    std::string errors;
    xmlSchemaSetValidErrors(validCtx, &detail::xsdErrorCollectorAlg, &detail::xsdErrorCollectorAlg, &errors);
    int result = xmlSchemaValidateDoc(validCtx, doc);

    xmlSchemaFreeValidCtxt(validCtx);
    xmlFreeDoc(doc);
    xmlSchemaFree(schema);

    if (result != 0) {
        if (errors.empty()) errors = "AI response XML does not conform to mc3.xsd";
        return errors;
    }
    return std::nullopt;
#else
    (void)xml;
    return std::nullopt;
#endif
}

// Result of validateAndParseAiResponseAlg(): either a usable document, or
// an error message explaining why the response was rejected.
struct AiResponseParseResultAlg {
    std::optional<Mc3::Mc3Document> doc;
    std::string                     errorMessage; // set only when doc is empty
};

// The full AI-response validation pipeline (mirrors the `else try { ... }`
// block in MeshCraftApplication::drawAiPanel(), MeshCraftApplication_UiAi.cpp,
// minus the ImGui-side line-number-diagnostic enrichment that's applied to
// errorMessage afterward): extract → repair → require a <mc3> root →
// parse → reject an empty document. Never throws.
inline AiResponseParseResultAlg validateAndParseAiResponseAlg(const std::string& rawResponse)
{
    AiResponseParseResultAlg r;
    std::string xml = repairXmlAlg(extractXmlAlg(rawResponse));
    if (xml.find("<mc3") == std::string::npos) {
        r.errorMessage = "Response does not contain a <mc3> root element";
        return r;
    }
    try {
        Mc3::Mc3Document parsed = parseXmlAlg(xml);
        if (isEmptyMc3DocumentAlg(parsed)) {
            r.errorMessage =
                "AI returned an empty document (no objects, no definitions). "
                "Not applying to avoid destroying the current scene.";
            return r;
        }
        if (auto xsdError = validateXmlAgainstXsdAlg(xml)) {
            r.errorMessage = "AI response does not conform to mc3.xsd: " + *xsdError;
            return r;
        }
        r.doc = std::move(parsed);
    } catch (const std::exception& ex) {
        r.errorMessage = ex.what();
    }
    return r;
}

} // namespace MeshCraft
