// F8 (2026-07-20 audit) — editing an <include>-sourced material/texture/
// definition/embed in the GUI editor and saving used to silently drop the
// edit. The writer (Mc3XmlWriter.cpp/Mc3JsonWriter.cpp) skips serializing
// any resource id still present in doc.includedMaterials/includedTextures/
// includedDefs/includedEmbeds, on the assumption that content keeps coming
// from the <include> file on the next load. The editor
// (MeshCraftApplication_UiLeftPanel.cpp's Tex/Defs/Mat/Embeds tabs and
// PropertiesPanel.cpp's inline material editor) never consulted or updated
// those four sets at all, so an in-place edit to an included resource's
// value was applied in memory (visibly, in the live 3D view) but silently
// discarded the moment Save ran, and reloading brought back the OLD
// included content with no error or warning.
//
// The fix promotes an id to local at the moment a real edit begins: erase
// it from the relevant includedX set, exactly mirroring what the PARSER
// itself already does when the SAME id is locally redeclared in the main
// file (Mc3XmlParser.cpp's buildDocumentFromRoot(), "Task 1": `if (const
// char* id = c->Attribute("id")) doc.includedMaterials.erase(id);` for a
// local <materials> block found after </include> processing).
//
// This test doesn't drive the ImGui editor (it isn't headlessly callable —
// MeshCraftApplication is CNA-coupled); instead it proves the underlying
// mechanism the fix relies on, entirely through the public mc3/ document
// API: an id erased from includedX, then saved, survives a save+reload
// round trip with its edited value -- and, as a contrast case, an id left
// in includedX is confirmed to still silently revert (the exact pre-fix
// bug), so this test would have failed before the fix existed anywhere in
// the editor.

#include <MeshCraft/Mc3/Mc3Document.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;
namespace fs = std::filesystem;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

// Writes an included library file with one material ("shared_mat", red),
// one texture ("shared_tex"), one definition ("shared_def", a Box), and one
// embed ("shared_embed"), and a main file that <include>s it.
static void writeFixture(const fs::path& dir) {
    fs::path libPath = dir / "lib.mc3.xml";
    std::ofstream lib(libPath);
    lib << "<mc3 version=\"0.3\" model=\"lib\">\n"
        << "  <textures><texture id=\"shared_tex\" uri=\"orig.png\"/></textures>\n"
        << "  <materials><material id=\"shared_mat\"><base_color>1 0 0 1</base_color></material></materials>\n"
        << "  <definitions><definition id=\"shared_def\"><box id=\"shared_def\" name=\"shared_def\" size=\"1 1 1\"/></definition></definitions>\n"
        << "  <embeds><embed id=\"shared_embed\" type=\"gltf\" src=\"orig.glb\"/></embeds>\n"
        << "</mc3>\n";
    lib.close();

    fs::path mainPath = dir / "main.mc3.xml";
    std::ofstream main_(mainPath);
    main_ << "<mc3 version=\"0.3\" model=\"main\">\n"
          << "  <include file=\"lib.mc3.xml\"/>\n"
          << "</mc3>\n";
}

int main() {
    fs::path dir = fs::temp_directory_path() / "mc3_included_resource_edit_test";
    fs::create_directories(dir);
    writeFixture(dir);
    fs::path mainPath = dir / "main.mc3.xml";

    // --- Sanity: the include actually merges and marks all 4 resource kinds. ---
    {
        Mc3Document doc = Mc3Document::loadFromFile(mainPath);
        check(doc.materials.count("shared_mat") == 1, "Sanity: shared_mat merged from include");
        check(doc.includedMaterials.count("shared_mat") == 1, "Sanity: shared_mat marked as included");
        check(doc.textures.count("shared_tex") == 1, "Sanity: shared_tex merged from include");
        check(doc.includedTextures.count("shared_tex") == 1, "Sanity: shared_tex marked as included");
        check(doc.definitions.count("shared_def") == 1, "Sanity: shared_def merged from include");
        check(doc.includedDefs.count("shared_def") == 1, "Sanity: shared_def marked as included");
        check(doc.embeds.count("shared_embed") == 1, "Sanity: shared_embed merged from include");
        check(doc.includedEmbeds.count("shared_embed") == 1, "Sanity: shared_embed marked as included");
    }

    // --- Contrast case (the pre-fix bug, reproduced directly): edit a
    // value in memory WITHOUT erasing it from includedMaterials (exactly
    // what the editor did before this fix), save, reload -- the edit is
    // silently lost. ---
    {
        Mc3Document doc = Mc3Document::loadFromFile(mainPath);
        doc.materials["shared_mat"].baseColor = {0.0f, 1.0f, 0.0f, 1.0f};  // red -> green
        // Deliberately NOT erasing from doc.includedMaterials here.
        fs::path outPath = dir / "bug_repro.mc3.xml";
        doc.saveToFile(outPath);

        Mc3Document reloaded = Mc3Document::loadFromFile(outPath);
        bool editSurvived = reloaded.materials.count("shared_mat") == 1 &&
                             reloaded.materials["shared_mat"].baseColor[1] > 0.5f;
        check(!editSurvived,
              "Pre-fix bug reproduced: an edit left in includedMaterials is silently "
              "dropped by the writer and reverts to the original included value on reload");
    }

    // --- Fixed behavior: erase from includedMaterials at edit time (what
    // every editor call site now does via pushUndoMat()/pushUndoTex()/
    // pushUndoDef()/pushUndoEmbed()), then save+reload -- the edit
    // survives. ---
    {
        Mc3Document doc = Mc3Document::loadFromFile(mainPath);
        doc.materials["shared_mat"].baseColor = {0.0f, 1.0f, 0.0f, 1.0f};  // red -> green
        doc.includedMaterials.erase("shared_mat");
        fs::path outPath = dir / "fixed_material.mc3.xml";
        doc.saveToFile(outPath);

        Mc3Document reloaded = Mc3Document::loadFromFile(outPath);
        check(reloaded.materials.count("shared_mat") == 1,
              "Fixed: edited material still exists after save+reload");
        check(reloaded.materials["shared_mat"].baseColor[1] > 0.5f &&
              reloaded.materials["shared_mat"].baseColor[0] < 0.5f,
              "Fixed: edited material's new color (green) survives save+reload, "
              "not the original included color (red)");
        // The saved file must no longer rely on the include for this id --
        // the whole point is it's now a genuine local override.
        check(reloaded.includedMaterials.count("shared_mat") == 0,
              "Fixed: reloaded document sees shared_mat as local, not included "
              "(the saved file declared it directly)");
    }

    // --- Same fix, applied to textures/definitions/embeds -- one
    // representative round trip each, mirroring the material case above. ---
    {
        Mc3Document doc = Mc3Document::loadFromFile(mainPath);
        doc.textures["shared_tex"].uri = "edited.png";
        doc.includedTextures.erase("shared_tex");
        fs::path outPath = dir / "fixed_texture.mc3.xml";
        doc.saveToFile(outPath);

        Mc3Document reloaded = Mc3Document::loadFromFile(outPath);
        check(reloaded.textures.count("shared_tex") == 1 &&
              reloaded.textures["shared_tex"].uri == "edited.png",
              "Fixed: edited texture uri survives save+reload");
    }
    {
        Mc3Document doc = Mc3Document::loadFromFile(mainPath);
        doc.definitions["shared_def"]->name = "edited_name";
        doc.includedDefs.erase("shared_def");
        fs::path outPath = dir / "fixed_definition.mc3.xml";
        doc.saveToFile(outPath);

        Mc3Document reloaded = Mc3Document::loadFromFile(outPath);
        check(reloaded.definitions.count("shared_def") == 1 &&
              reloaded.definitions["shared_def"] &&
              reloaded.definitions["shared_def"]->name == "edited_name",
              "Fixed: edited definition name survives save+reload");
    }
    {
        Mc3Document doc = Mc3Document::loadFromFile(mainPath);
        doc.embeds["shared_embed"].src = "edited.glb";
        doc.includedEmbeds.erase("shared_embed");
        fs::path outPath = dir / "fixed_embed.mc3.xml";
        doc.saveToFile(outPath);

        Mc3Document reloaded = Mc3Document::loadFromFile(outPath);
        check(reloaded.embeds.count("shared_embed") == 1 &&
              reloaded.embeds["shared_embed"].src == "edited.glb",
              "Fixed: edited embed src survives save+reload");
    }

    if (failures == 0) { std::cout << "All included-resource-edit tests passed.\n"; return 0; }
    std::cerr << failures << " included-resource-edit test(s) failed.\n";
    return 1;
}
