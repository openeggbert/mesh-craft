#pragma once

#include "MeshCraft/Mc3/Mc3Document.hpp"

#include <string>

namespace MeshCraft::Editor {

// SYS-W14-18 (2026-07-20) -- executes a script (Mc3Script::source,
// type "lua") against a document. Mc3Object::scriptId/doc.scripts already
// parsed/serialized/round-tripped/were editable in the UI, but nothing
// anywhere ever interpreted a script's source text.
//
// Mirrors the sibling ../mesh-world repo's own already-working, sandboxed
// setup (Mc3ScriptRunner.cpp/LuaRuntime.cpp -- Lua 5.4 + sol2 v3.3.0,
// same library choice/sandbox discipline) rather than inventing a
// different one, per explicit instruction. Two globals are bound while a
// script runs:
//
//   def -- the R103/R104 "compose-time placement" API mesh-world's own
//     Mc3ScriptRunner defines, unchanged: def:place(childId,
//     definitionRef, socketName) / def:place_at(childId, definitionRef,
//     x, y, z) / def:has_socket(socketName) -> bool, operating on
//     `target` (must already exist in doc.definitions, post-
//     Mc3ImportResolver resolution, for a definitionRef to resolve).
//     If `target` is null, place()/place_at() report a clear error
//     instead of crashing or silently no-op'ing.
//
//   scene -- broader than mesh-world's own scope (this repo's own
//     addition, per explicit request for read/write object property
//     access, not just socket placement): scene:find(nameOrId) looks up
//     ANY object in the whole document (by id first, then by name) and
//     returns a handle exposing get_position()/set_position(x,y,z),
//     get_rotation()/set_rotation(x,y,z), get_scale()/set_scale(x,y,z),
//     get_visible()/set_visible(bool), get_material()/set_material(id),
//     plus read-only .name/.id -- or nil if nothing matches.
//
// Same sandboxing discipline as mesh-world's LuaRuntime/Mc3ScriptRunner:
// only base/math/string/table Lua libraries; io/os/debug/package/
// dofile/loadfile/load/collectgarbage/require are all removed/blocked.
// ALSO adds an instruction-count execution limit mesh-world's own
// reference implementation doesn't have (acceptable there -- an
// offline/CLI tool; not acceptable here -- an infinite Lua loop must not
// hang the whole interactive editor UI thread).
class LuaScriptRunner {
public:
    // Fresh sol::state per call, no persistent state carried between runs
    // (matches mesh-world's own Mc3ScriptRunner::run()). `target` may be
    // nullptr (there is no single well-defined "current object" for
    // e.g. a trigger's run-script step or the Scripts tab's own preview
    // button when nothing is selected) -- def:place()/place_at() then
    // report a clear error rather than crashing. Mutates `doc` (and
    // `target`, if non-null and it's one of doc's own objects) in place.
    // An empty `source` is a legitimate no-op (matches
    // Mc3Script::hasSource()'s own established convention), not an error.
    // Returns "" on success, an error description otherwise -- never throws.
    std::string run(const std::string& source, Mc3::Mc3Document& doc, Mc3::Mc3Object* target);
};

} // namespace MeshCraft::Editor
