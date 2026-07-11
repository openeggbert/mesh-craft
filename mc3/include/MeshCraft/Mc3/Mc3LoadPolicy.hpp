#pragma once

#include <cstddef>

namespace MeshCraft::Mc3 {

// Controls how much a parse is allowed to touch the outside world.
//
// Trusted user files (opened by the editor from the user's own disk) keep the
// full, permissive behavior. Untrusted content — an AI-generated scene, a
// pasted/imported document, anything not authored locally — must be parsed with
// a restricted policy so it cannot use <include file="..."> to open and merge
// arbitrary local files (a local-file-inclusion vector) or nest includes
// without bound.
struct Mc3LoadPolicy {
    // Process <include file="..."> elements. When false, includes are ignored
    // (a diagnostic is emitted) instead of resolved.
    bool allowIncludes = true;

    // Reject include paths that are absolute or escape the document's own
    // directory via `..`. Applies only when includes are processed at all.
    bool confineIncludesToRoot = false;

    // Hard cap on nested-include depth. Cycle detection already prevents true
    // infinite recursion, but a deep linear chain is still bounded here.
    int maxIncludeDepth = 16;

    // Permissive policy: current behavior for trusted, locally-authored files.
    static Mc3LoadPolicy trusted() { return Mc3LoadPolicy{}; }

    // Restricted policy for untrusted content (AI output, imports): no
    // filesystem includes at all, and path confinement on if they were ever
    // re-enabled.
    static Mc3LoadPolicy untrusted() {
        Mc3LoadPolicy p;
        p.allowIncludes = false;
        p.confineIncludesToRoot = true;
        p.maxIncludeDepth = 0;
        return p;
    }
};

} // namespace MeshCraft::Mc3
