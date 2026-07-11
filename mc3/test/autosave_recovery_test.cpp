// Crash-recovery mechanism test (SYS-W9-02).
//
// MeshCraftApplication::checkForNewerAutosave()/recoverFromAutosave() (see
// src/MeshCraft/MeshCraftApplication_FileOps.cpp) decide whether to offer
// recovery by comparing the saved file's mtime against its ".autosave"
// sibling's mtime, and recover by loading the autosave's content in place
// of the saved file's. Those App-level methods are CNA-coupled (SDL/ImGui)
// and not headlessly testable directly, but the mechanism they rely on --
// mtime comparison + Mc3Document round-trip through the ".autosave" path
// convention -- is pure filesystem + Mc3 and is fully testable here.

#include <MeshCraft/Mc3/Mc3Document.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

static void writeXml(const std::filesystem::path& path, const std::string& model) {
    std::ofstream f(path);
    f << "<mc3 version=\"0.3\" model=\"" << model << "\">\n  <objects/>\n</mc3>\n";
}

int main() {
    const auto dir = std::filesystem::temp_directory_path();
    const auto file     = dir / "mc3_autosave_recovery_test.mc3.xml";
    const auto autosave = std::filesystem::path(file.string() + ".autosave"); // matches autoSavePathAlg

    std::filesystem::remove(file);
    std::filesystem::remove(autosave);

    // --- Scenario 1: autosave newer than the saved file (crash recovery case) ---
    {
        writeXml(file, "Saved");
        // Force a real filesystem-clock tick between writes -- some
        // filesystems have coarse mtime resolution (as low as 1s on some
        // configurations), so a same-instant second write can otherwise
        // compare equal rather than strictly greater.
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        writeXml(autosave, "Recovered");

        std::error_code ec;
        auto savedTime = std::filesystem::last_write_time(file, ec);
        auto asTime    = std::filesystem::last_write_time(autosave, ec);
        check(!ec, "scenario 1: both mtimes read without error");
        check(asTime > savedTime, "scenario 1: autosave mtime is strictly newer than the saved file's");

        // recoverFromAutosave() loads the .autosave path's content, not the
        // real file's -- verify that produces the RECOVERED content.
        Mc3Document recovered = Mc3Document::loadFromFile(autosave);
        check(recovered.model == "Recovered", "scenario 1: loading the .autosave path yields the recovered content");

        // The saved file itself is untouched by the check/detection step.
        Mc3Document saved = Mc3Document::loadFromFile(file);
        check(saved.model == "Saved", "scenario 1: the original saved file is untouched by detection");
    }

    // --- Scenario 2: autosave older than (or same age as) the saved file --
    // (a normal save cleaned up the autosave, or the user saved again after
    // an old leftover autosave -- no recovery should be offered) ---
    {
        std::filesystem::remove(file);
        std::filesystem::remove(autosave);
        writeXml(autosave, "StaleRecovered");
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        writeXml(file, "Saved2"); // saved AFTER the stale autosave -> file is newer

        std::error_code ec;
        auto savedTime = std::filesystem::last_write_time(file, ec);
        auto asTime    = std::filesystem::last_write_time(autosave, ec);
        check(!(asTime > savedTime),
              "scenario 2: a stale autosave older than the saved file does not compare newer "
              "(checkForNewerAutosave's condition correctly stays false)");
    }

    // --- Scenario 3: no autosave sibling at all (the common case) ---
    {
        std::filesystem::remove(file);
        std::filesystem::remove(autosave);
        writeXml(file, "Saved3");
        std::error_code ec;
        check(!std::filesystem::exists(autosave, ec),
              "scenario 3: no .autosave sibling exists -- checkForNewerAutosave's exists() guard short-circuits");
    }

    // --- Scenario 4: discardAutosave()'s mechanism -- removing the sibling
    // leaves the saved file exactly as it was ---
    {
        std::filesystem::remove(file);
        std::filesystem::remove(autosave);
        writeXml(file, "Saved4");
        writeXml(autosave, "Discarded4");
        std::error_code ec;
        std::filesystem::remove(autosave, ec);
        check(!ec, "scenario 4: removing the .autosave sibling (discardAutosave's mechanism) succeeds");
        check(!std::filesystem::exists(autosave, ec), "scenario 4: .autosave sibling is gone after discard");
        Mc3Document stillSaved = Mc3Document::loadFromFile(file);
        check(stillSaved.model == "Saved4", "scenario 4: the saved file itself is unaffected by discarding the autosave");
    }

    std::filesystem::remove(file);
    std::filesystem::remove(autosave);

    if (failures == 0) { std::cout << "All autosave-recovery tests passed.\n"; return 0; }
    std::cerr << failures << " autosave-recovery test(s) failed.\n";
    return 1;
}
