#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace MeshCraft::Mc3 {

// SYS-W1-01: first-class validation diagnostics for MC3/MCB load (and,
// eventually, the other integration points named in plan.md). This is a pure
// data type with no CNA dependency, deliberately mirroring the layering of
// the rest of mc3/include -- it can be included from CNA-free code (mc3togltf,
// command-line tools) as well as the editor.
//
// Design note (see the commit that introduces this file for the full
// rationale): this is an ADDITIVE side-channel, not a replacement for the
// existing throw-on-hard-rejection / clamp-on-recoverable-issue behavior
// already present in Mc3XmlParser.cpp and McbReader.cpp. Callers who don't
// pass an Mc3Validation keep exactly today's behavior (exceptions for hard
// rejections, silent clamps for recoverable ones). Callers who DO pass one
// additionally get a structured entry for every warning/error the operation
// produces -- including entries for the hard-rejection cases, recorded
// immediately before the corresponding exception is thrown, so a caller that
// catches the exception can still inspect *why* via the Mc3Validation object
// (which, being passed by reference/pointer rather than returned, retains
// whatever was appended to it even though the function that was populating it
// never returned normally).
enum class Mc3ValidationSeverity {
    Warning, // content was accepted; something was clamped, defaulted, or
             // otherwise silently repaired.
    Error,   // the operation was (or will be) rejected outright.
};

// A single validation finding.
struct Mc3ValidationEntry {
    Mc3ValidationSeverity severity = Mc3ValidationSeverity::Warning;

    // Path of the file the finding came from -- the top-level document being
    // loaded, or an <include>d file merged into it. May be empty when the
    // finding isn't tied to a specific file on disk (e.g. an in-memory
    // parseString() call with no sourceDir) or the source wasn't threaded
    // through to the call site yet.
    std::string sourcePath;

    // Identity of the object the finding concerns, when one is available --
    // typically the element's `id` attribute, falling back to `name`, falling
    // back to the tag name. Left empty (not synthesized) when no identity is
    // available, e.g. for whole-document findings like a budget overflow.
    std::string objectId;

    // Attribute/field name the finding concerns (e.g. "segments",
    // "roughness"). Empty for whole-document findings.
    std::string field;

    // Human-readable explanation of what was found.
    std::string message;

    // What was done about it automatically, if anything (e.g. "clamped to
    // 4096", "replaced with 0.0"). Empty when the finding is a hard
    // rejection with no repair, or when no repair applies.
    std::string suggestedRepair;
};

// Accumulates Mc3ValidationEntry findings across a single operation (a load,
// an include-merge, a save, ...). Passing the same Mc3Validation instance
// into several operations accumulates findings from all of them -- e.g. a
// single load's top-level document plus every file it <include>s.
class Mc3Validation {
public:
    std::vector<Mc3ValidationEntry> entries;

    void addWarning(std::string sourcePath, std::string objectId, std::string field,
                     std::string message, std::string suggestedRepair = {}) {
        entries.push_back(Mc3ValidationEntry{
            Mc3ValidationSeverity::Warning, std::move(sourcePath), std::move(objectId),
            std::move(field), std::move(message), std::move(suggestedRepair)});
    }

    void addError(std::string sourcePath, std::string objectId, std::string field,
                   std::string message, std::string suggestedRepair = {}) {
        entries.push_back(Mc3ValidationEntry{
            Mc3ValidationSeverity::Error, std::move(sourcePath), std::move(objectId),
            std::move(field), std::move(message), std::move(suggestedRepair)});
    }

    bool empty() const { return entries.empty(); }

    bool hasErrors() const {
        for (const auto& e : entries)
            if (e.severity == Mc3ValidationSeverity::Error) return true;
        return false;
    }

    bool hasWarnings() const {
        for (const auto& e : entries)
            if (e.severity == Mc3ValidationSeverity::Warning) return true;
        return false;
    }

    size_t errorCount() const {
        size_t n = 0;
        for (const auto& e : entries)
            if (e.severity == Mc3ValidationSeverity::Error) ++n;
        return n;
    }

    size_t warningCount() const {
        size_t n = 0;
        for (const auto& e : entries)
            if (e.severity == Mc3ValidationSeverity::Warning) ++n;
        return n;
    }

    void clear() { entries.clear(); }

    // Append another Mc3Validation's entries onto this one.
    void merge(const Mc3Validation& other) {
        entries.insert(entries.end(), other.entries.begin(), other.entries.end());
    }
};

} // namespace MeshCraft::Mc3
