// F9 (2026-07-20 audit) — Browse ("..." button, SYS-W14-15) and OS
// drag-drop both used to assign a material texture-slot field
// (baseColorTexture etc.) the RAW FILE PATH the OS gave back, e.g.
// "/home/user/textures/wood.png". Both the live renderer
// (SceneRenderer.cpp: `doc.textures.find(matIt->second.baseColorTexture)`)
// and the glTF exporter (GltfExporter.cpp: `texIdx.find(mat.baseColorTexture)`)
// resolve that field as a doc.textures KEY, not a path -- so the lookup
// silently failed (texture treated as absent) even though the assignment
// itself appeared to succeed in the UI.
//
// The fix, registerTextureFromPathAlg() (EditorAlgorithms.hpp), registers
// (or reuses) a doc.textures entry for the dropped/browsed path and
// returns its id, which every Browse/drag-drop call site now assigns to
// the material field instead of the raw path.
//
// CNA-free: EditorAlgorithms.hpp only needs Mc3Document/Mc3Object
// (header-only), matching object_index_test/macro_recorder_test's
// precedent -- no Mc3/CNA linking required.

#include "MeshCraft/EditorCommandAlgorithms.hpp"

#include <cstdio>

using namespace MeshCraft;
using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) std::printf("PASS: %s\n", msg.c_str());
    else      { std::printf("FAIL: %s\n", msg.c_str()); ++failures; }
}

int main() {
    // Fresh path: creates a new doc.textures entry, uri == the raw path.
    {
        Mc3Document doc;
        std::string id = registerTextureFromPathAlg("/home/user/textures/wood.png", doc);
        check(!id.empty(), "New path: returns a non-empty id");
        check(doc.textures.count(id) == 1, "New path: a doc.textures entry was created under that id");
        check(doc.textures[id].uri == "/home/user/textures/wood.png",
              "New path: the entry's uri is the original raw path");
    }

    // The exact bug this fixes, reproduced directly: assigning the raw path
    // straight into a material field (pre-fix behavior) means the
    // renderer's/exporter's own doc.textures.find() lookup silently fails.
    {
        Mc3Document doc;
        registerTextureFromPathAlg("/textures/a.png", doc); // some unrelated entry
        std::string rawPath = "/home/user/textures/wood.png";
        // Pre-fix: mat.baseColorTexture = rawPath directly.
        bool preFixResolves = doc.textures.count(rawPath) > 0;
        check(!preFixResolves,
              "Pre-fix bug reproduced: doc.textures.find(rawPath) fails to resolve a raw "
              "path assigned directly, exactly matching the renderer's/exporter's lookup");

        // Fixed: mat.baseColorTexture = registerTextureFromPathAlg(rawPath, doc).
        std::string id = registerTextureFromPathAlg(rawPath, doc);
        bool fixedResolves = doc.textures.count(id) > 0 && doc.textures[id].uri == rawPath;
        check(fixedResolves,
              "Fixed: doc.textures.find(id) resolves correctly to the registered entry, "
              "mirroring SceneRenderer.cpp's and GltfExporter.cpp's own lookup");
    }

    // Re-registering the SAME path returns the SAME id, no duplicate entry
    // -- repeatedly Browse-ing or dropping the same file onto a slot must
    // not spawn a fresh doc.textures entry every time.
    {
        Mc3Document doc;
        std::string id1 = registerTextureFromPathAlg("/tex/metal.png", doc);
        std::string id2 = registerTextureFromPathAlg("/tex/metal.png", doc);
        check(id1 == id2, "Same path registered twice: returns the same id both times");
        check(doc.textures.size() == 1, "Same path registered twice: only one doc.textures entry exists");
    }

    // Two DIFFERENT paths that happen to share a filename (stem) must not
    // collide -- both get their own entry with distinct ids.
    {
        Mc3Document doc;
        std::string id1 = registerTextureFromPathAlg("/a/wood.png", doc);
        std::string id2 = registerTextureFromPathAlg("/b/wood.png", doc);
        check(id1 != id2, "Different paths sharing a filename: distinct ids assigned");
        check(doc.textures.size() == 2, "Different paths sharing a filename: two separate entries exist");
        check(doc.textures[id1].uri == "/a/wood.png" && doc.textures[id2].uri == "/b/wood.png",
              "Different paths sharing a filename: each entry keeps its own original uri");
    }

    // A path with no discoverable stem (e.g. empty) still gets a usable id
    // rather than an empty/invalid map key.
    {
        Mc3Document doc;
        std::string id = registerTextureFromPathAlg("", doc);
        check(!id.empty(), "Empty path: still returns a non-empty fallback id");
        check(doc.textures.count(id) == 1, "Empty path: a doc.textures entry was still created");
    }

    if (failures == 0) { std::printf("All texture-from-path tests passed.\n"); return 0; }
    std::fprintf(stderr, "%d texture-from-path test(s) failed.\n", failures);
    return 1;
}
