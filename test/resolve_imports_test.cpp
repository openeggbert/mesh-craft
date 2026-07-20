// SYS-W14-21 (2026-07-20): MeshCraftApplication::resolveImports() wires the
// already-tested Mc3ImportResolver (R101, mc3/src/Mc3ImportResolver.cpp,
// exhaustively covered by mc3/test/import_resolver_test.cpp) into the
// editor's load paths (initial launch, Open dialog, OpenRecentFile,
// autosave recovery) plus an explicit "Resolve Imports" button in the
// Imports tab. MeshCraftApplication itself is CNA-coupled and not
// headlessly instantiable, so this mirrors resolveImports()'s exact
// control-flow shape (empty-imports no-op, try/resolveAndMergeInto/catch,
// status message construction) against a real Mc3Document and the real
// Mc3ImportResolver -- matching trigger_fire_test.cpp/scene_state_apply_test.cpp's
// established pattern for this session's other three UI-wired features.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3ImportResolver.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>

#include <filesystem>
#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while (0)

namespace {

std::filesystem::path testDir() {
    auto dir = std::filesystem::temp_directory_path() / "resolve_imports_test";
    std::filesystem::create_directories(dir);
    return dir;
}

std::shared_ptr<Mc3Object> makeBox(float size) {
    auto box = std::make_shared<Mc3Object>();
    box->type = ObjectType::Box;
    box->primitive = Mc3Primitive::box({size, size, size});
    return box;
}

void writeLibrary(const std::string& name, const std::string& version, const std::string& defId) {
    Mc3Document doc;
    doc.model = name;
    doc.library = Mc3LibraryInfo{name, version, ""};
    doc.defineObject(defId, makeBox(1.0f));
    doc.library->contentHash = "sha256:" + doc.computeLibraryContentHash();
    doc.saveToLibraryFile(testDir() / (name + "-" + version + ".mc3lib.xml"));
}

// Mirrors MeshCraftApplication::resolveImports() (MeshCraftApplication_FileOps.cpp)
// exactly: empty-imports no-op, try resolveAndMergeInto()/catch, and the same
// two status-message shapes -- but recording onto plain trackers instead of
// calling the real (CNA-coupled) setStatusMsg().
struct StatusTracker {
    bool called = false;
    std::string msg;
    bool isError = false;
};

void resolveImportsMirror(Mc3Document& document, StatusTracker& status) {
    if (document.imports.empty()) return;
    try {
        Mc3ImportResolver resolver({document.sourcePath});
        resolver.resolveAndMergeInto(document);
        status.called = true;
        status.isError = false;
        status.msg = "Resolved " + std::to_string(document.imports.size()) +
                     " import" + (document.imports.size() == 1 ? "" : "s");
    } catch (const std::exception& e) {
        status.called = true;
        status.isError = true;
        status.msg = std::string("Import resolution failed: ") + e.what();
    }
}

} // namespace

int main() {
    // --- 1. Empty imports: no-op. No resolver constructed, no status set,
    //        document.definitions untouched. ---
    {
        Mc3Document doc;
        doc.model = "scene";
        doc.sourcePath = testDir();
        StatusTracker status;

        resolveImportsMirror(doc, status);

        CHECK(!status.called, "empty imports: no status message is set");
        CHECK(doc.definitions.empty(), "empty imports: definitions map stays empty");
    }

    // --- 2. Successful resolution: a real library on disk resolves and its
    //        definition is merged into document.definitions. ---
    {
        writeLibrary("resolve-ok-lib", "1.0.0", "door.simple");

        Mc3Document doc;
        doc.model = "scene";
        doc.sourcePath = testDir();
        doc.imports.push_back(Mc3Import{"door_lib", "mc3lib://resolve-ok-lib@1.0.0", ""});
        StatusTracker status;

        resolveImportsMirror(doc, status);

        CHECK(status.called, "success: status message is set");
        CHECK(!status.isError, "success: status is not an error");
        CHECK(status.msg == "Resolved 1 import", "success: status message text matches");
        CHECK(doc.definitions.count("door_lib:door.simple") == 1,
              "success: imported definition merged into document.definitions");
    }

    // --- 3. Failure (missing library file): resolveAndMergeInto() throws,
    //        but resolveImportsMirror() catches it -- doesn't propagate, and
    //        reports via the error status shape instead. Matches the
    //        documented "a recoverable data issue shouldn't make an
    //        otherwise-loadable document unopenable" contract. ---
    {
        Mc3Document doc;
        doc.model = "scene";
        doc.sourcePath = testDir();
        doc.imports.push_back(Mc3Import{"missing", "mc3lib://does-not-exist@9.9.9", ""});
        StatusTracker status;

        bool threw = false;
        try { resolveImportsMirror(doc, status); }
        catch (...) { threw = true; }

        CHECK(!threw, "failure: resolution error does not propagate out of resolveImports()");
        CHECK(status.called, "failure: status message is still set");
        CHECK(status.isError, "failure: status is reported as an error");
        CHECK(status.msg.find("Import resolution failed") != std::string::npos,
              "failure: status message uses the documented error prefix");
        CHECK(doc.definitions.empty(),
              "failure: definitions map stays empty (unresolved import, not a partial merge)");
    }

    // --- 4. Multiple imports pluralization in the success message. ---
    {
        writeLibrary("resolve-ok-lib-2", "1.0.0", "window.simple");

        Mc3Document doc;
        doc.model = "scene";
        doc.sourcePath = testDir();
        doc.imports.push_back(Mc3Import{"door_lib", "mc3lib://resolve-ok-lib@1.0.0", ""});
        doc.imports.push_back(Mc3Import{"window_lib", "mc3lib://resolve-ok-lib-2@1.0.0", ""});
        StatusTracker status;

        resolveImportsMirror(doc, status);

        CHECK(!status.isError, "pluralization: two valid imports resolve without error");
        CHECK(status.msg == "Resolved 2 imports", "pluralization: status message uses plural form");
    }

    if (failures == 0)
        std::cout << "All resolveImports() (SYS-W14-21) tests passed.\n";
    else
        std::cerr << failures << " resolveImports() (SYS-W14-21) test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
