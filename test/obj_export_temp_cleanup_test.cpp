// F19 (2026-07-20 audit) — MeshCraftApplication::runObjExport()'s
// intermediate .glb temp path used to be derived from `this`'s pointer
// value alone (deterministic -- two concurrent MeshCraft instances
// exporting OBJ around the same time could collide on the exact same
// filename), and the temp file was only ever removed on the success path
// -- exportDocument()/LoadBinaryFromFile() throwing left it behind
// forever. Fixed with uniqueTempPath() (already used by
// ModelRegistry.cpp/MeshCraftApplication_UiAi.cpp for exactly this
// class of bug) plus a try/catch that removes the temp file before
// rethrowing.
//
// runObjExport() itself is a MeshCraftApplication member (CNA-coupled,
// not headlessly instantiable) that also depends on mc3togltf::GltfExporter/
// tinygltf, so this mirrors its exact fixed control-flow shape -- real
// uniqueTempPath() + real std::filesystem operations, no CNA/tinygltf
// dependency -- matching this session's established pattern for testing
// CNA-coupled code (F2/F5/F7's own test files use the same approach).

#include "MeshCraft/TempFile.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>

using namespace MeshCraft;
namespace fs = std::filesystem;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) std::printf("PASS: %s\n", msg.c_str());
    else      { std::printf("FAIL: %s\n", msg.c_str()); ++failures; }
}

// Mirrors runObjExport()'s fixed shape: creates the temp file, runs `work`
// (simulating exportDocument()+LoadBinaryFromFile()), removes the temp
// file whether `work` throws or returns normally.
static void runWithTempCleanup(const fs::path& tempPath, const std::function<void()>& work) {
    std::ofstream(tempPath) << "fake glb bytes";
    std::error_code ec;
    try {
        work();
        fs::remove(tempPath, ec);
    } catch (...) {
        fs::remove(tempPath, ec);
        throw;
    }
}

int main() {
    // F19a: uniqueTempPath() must not collide across "concurrent instances"
    // -- the exact property the fix replaces a this-pointer-derived name
    // with. Same prefix/extension runObjExport() itself uses.
    {
        std::set<std::string> seen;
        bool anyCollision = false;
        for (int i = 0; i < 100; ++i) {
            auto p = uniqueTempPath("meshcraft_objexport", ".glb");
            if (!seen.insert(p.string()).second) anyCollision = true;
        }
        check(!anyCollision,
              "F19a: 100 calls to uniqueTempPath(\"meshcraft_objexport\", \".glb\") "
              "produce 100 distinct paths (no this-pointer-style determinism)");
    }

    // F19b: success path removes the temp file (baseline -- this part
    // already worked pre-fix, confirms the mirror itself is faithful).
    {
        auto tmp = uniqueTempPath("objexport_success_test", ".glb");
        runWithTempCleanup(tmp, [] { /* simulate a successful export+reload */ });
        check(!fs::exists(tmp), "F19b: temp file is removed after a successful export");
    }

    // F19c (the actual pre-fix bug, reproduced directly): if `work` throws
    // partway through (mirrors exportDocument()/LoadBinaryFromFile()
    // failing), the temp file must STILL be removed, and the exception
    // must still propagate to the caller (runObjExport()'s own caller
    // catches it to show objExportErr_ in the UI -- swallowing it here
    // would be its own bug).
    {
        auto tmp = uniqueTempPath("objexport_failure_test", ".glb");
        bool threw = false;
        std::string what;
        try {
            runWithTempCleanup(tmp, [] {
                throw std::runtime_error("Failed to re-read intermediate GLB for OBJ export: boom");
            });
        } catch (const std::exception& e) {
            threw = true;
            what = e.what();
        }
        check(threw, "F19c: the exception still propagates to the caller (not swallowed)");
        check(what.find("boom") != std::string::npos,
              "F19c: the original exception's message is preserved, not replaced");
        check(!fs::exists(tmp),
              "F19c: temp file is removed even though the export step threw "
              "(the pre-fix bug: the removal only ran on the success path, "
              "leaking the .glb on every failed export)");
    }

    if (failures == 0) { std::printf("All OBJ-export temp-cleanup tests passed.\n"); return 0; }
    std::fprintf(stderr, "%d OBJ-export temp-cleanup test(s) failed.\n", failures);
    return 1;
}
