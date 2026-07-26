// Never-saved ("Untitled") document crash-recovery mechanism test
// (SYS-W9-07).
//
// MeshCraftApplication::performUntitledRecoverySave()/
// checkForUntitledRecovery()/recoverUntitledScene()/discardUntitledRecovery()
// (see src/MeshCraft/Application/FileOps.cpp) are CNA-coupled (SDL/ImGui)
// and not headlessly testable directly -- same limitation as
// autosave_recovery_test.cpp's own header comment for the named-file case.
// What IS pure filesystem + Mc3 and fully testable here is the mechanism
// those methods reduce to: write the in-memory document to a single bounded
// recovery path, later either load it back (recover) or remove it
// (discard), and survive a corrupt/garbage file at that path without
// crashing. `untitledRecoveryPath()` itself (src/MeshCraft/
// MeshCraftPrivate.hpp) lives in the config directory rather than beside a
// real file, since an untitled document has no real path to place a
// sibling next to -- this test uses a stand-in path in the same shape.

#include <MeshCraft/Mc3/Mc3Document.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

int main() {
    const auto dir = std::filesystem::temp_directory_path();
    // Stand-in for meshcraftConfigDir() / "untitled.recovery.mc3.xml" -- same
    // shape (a single fixed path, not a sibling of any real document file).
    const auto recoveryPath = dir / "mc3_untitled_recovery_test.mc3.xml";
    std::filesystem::remove(recoveryPath);

    // --- Modified untitled recovery: the periodic save writes real content,
    // and loading it back (what recoverUntitledScene() does) yields it. ---
    {
        Mc3Document doc;
        doc.model = "UntitledWorkInProgress";
        doc.addObject(Mc3Object::makeSphere("Sphere", 1.0f, 16));
        doc.saveToFile(recoveryPath);

        check(std::filesystem::exists(recoveryPath),
              "performUntitledRecoverySave()'s mechanism: the recovery file exists after saving");

        Mc3Document recovered = Mc3Document::loadFromFile(recoveryPath);
        check(recovered.model == "UntitledWorkInProgress",
              "recoverUntitledScene()'s mechanism: loading the recovery path yields the saved content");
        check(recovered.objects.size() == 1 && recovered.objects[0] &&
              recovered.objects[0]->name == "Sphere",
              "recoverUntitledScene()'s mechanism: the recovered document's objects round-trip");
    }

    // --- Discard: removing the file makes it as if nothing were ever
    // there -- checkForUntitledRecovery()'s existence check next startup
    // correctly finds nothing to offer. ---
    {
        check(std::filesystem::exists(recoveryPath), "discard scenario: recovery file exists before discard");
        std::error_code ec;
        std::filesystem::remove(recoveryPath, ec);
        check(!ec, "discardUntitledRecovery()'s mechanism: remove() succeeds without error");
        check(!std::filesystem::exists(recoveryPath),
              "discardUntitledRecovery()'s mechanism: the recovery file no longer exists");
    }

    // --- Clean shutdown / successful Save As cleanup: both reduce to the
    // same "no recovery file left behind" postcondition as discard, just
    // triggered from a different call site (the destructor, or the Save As
    // dialog handler) -- already covered by the discard case above at the
    // filesystem-mechanism level; the *triggering condition* (currentFile_
    // empty at shutdown, or transitioning away from empty on a successful
    // save) is App-level state this test cannot exercise headlessly. ---

    // --- Crash-marker simulation: a recovery file surviving without ever
    // being cleaned up is exactly what an abnormal termination (crash, kill,
    // power loss) looks like, since nothing ran to remove it. Simulated here
    // by simply leaving a file in place and confirming existence is
    // detectable -- checkForUntitledRecovery()'s own logic is a bare
    // exists() check, so this is the whole mechanism. ---
    {
        Mc3Document doc;
        doc.model = "SurvivedACrash";
        doc.saveToFile(recoveryPath);
        check(std::filesystem::exists(recoveryPath),
              "crash-marker simulation: a file left behind by an unclean exit is detectable");
        std::filesystem::remove(recoveryPath);
    }

    // --- Corrupt recovery file: must fail loudly (a catchable exception),
    // not crash -- recoverUntitledScene() wraps loadFromFile() in exactly
    // this try/catch. ---
    {
        { std::ofstream f(recoveryPath, std::ios::binary); f << "not xml at all {{{"; }
        bool threw = false;
        try {
            (void)Mc3Document::loadFromFile(recoveryPath);
        } catch (const std::exception&) {
            threw = true;
        }
        check(threw, "corrupt recovery file: loading it throws a catchable exception, not a crash");
        std::filesystem::remove(recoveryPath);
    }

    // --- Coexistence with the existing sibling .autosave recovery
    // (SYS-W9-02): the two mechanisms use disjoint path shapes by
    // construction (config-directory single slot vs. a real file's own
    // "<file>.autosave" sibling), so writing both for the same logical
    // session cannot collide. ---
    {
        const auto namedFile     = dir / "mc3_untitled_recovery_test_named.mc3.xml";
        const auto namedAutosave = std::filesystem::path(namedFile.string() + ".autosave");
        std::filesystem::remove(namedFile);
        std::filesystem::remove(namedAutosave);

        Mc3Document untitledDoc;
        untitledDoc.model = "UntitledSlot";
        untitledDoc.saveToFile(recoveryPath);

        Mc3Document namedDoc;
        namedDoc.model = "NamedSaved";
        namedDoc.saveToFile(namedFile);
        Mc3Document namedAutosaveDoc;
        namedAutosaveDoc.model = "NamedAutosaved";
        namedAutosaveDoc.saveToFile(namedAutosave);

        check(recoveryPath != namedAutosave && recoveryPath != namedFile,
              "coexistence: the untitled-recovery path is distinct from a named file's own "
              "path and its .autosave sibling");
        check(Mc3Document::loadFromFile(recoveryPath).model == "UntitledSlot" &&
              Mc3Document::loadFromFile(namedFile).model == "NamedSaved" &&
              Mc3Document::loadFromFile(namedAutosave).model == "NamedAutosaved",
              "coexistence: all three files retain their own independent content");

        std::filesystem::remove(recoveryPath);
        std::filesystem::remove(namedFile);
        std::filesystem::remove(namedAutosave);
    }

    if (failures == 0) { std::cout << "All untitled-recovery tests passed.\n"; return 0; }
    std::cerr << failures << " untitled-recovery test(s) failed.\n";
    return 1;
}
