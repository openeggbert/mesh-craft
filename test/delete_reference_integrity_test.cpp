// F10 (2026-07-20 audit) — Rename already rewrites every live reference to
// a shared resource's OLD id over to its NEW one (the Mat/Defs tabs' own
// fixRefs lambdas, ModelRegistry.cpp's remapMaterialRefs()). Delete had no
// equivalent: erasing a material/definition/texture/script/sound/music/
// embed left every object/material/trigger that referenced it pointing at
// a now-nonexistent id, with no warning and no cleanup.
//
// Covers the 8 clear*ReferencesAlg()/removeTriggerStepsReferencingAlg()
// functions (EditorAlgorithms.hpp) that MeshCraftApplication_UiLeftPanel.cpp's
// 8 Delete buttons now call before erasing. CNA-free -- EditorAlgorithms.hpp
// only needs Mc3Document/Mc3Object (header-only), matching
// object_index_test/texture_from_path_test's precedent.

#include "MeshCraft/EditorAlgorithms.hpp"

#include <cstdio>

using namespace MeshCraft;
using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) std::printf("PASS: %s\n", msg.c_str());
    else      { std::printf("FAIL: %s\n", msg.c_str()); ++failures; }
}

static std::shared_ptr<Mc3Object> makeObj(const std::string& id) {
    auto o = std::make_shared<Mc3Object>();
    o->id = id;
    return o;
}

int main() {
    // --- Materials: material, materialOverride, per-object state override,
    // scene-state override, all cleared; nested (child + definition) too. ---
    {
        Mc3Document doc;
        auto root = makeObj("root");
        root->material = "matA";
        auto child = makeObj("child");
        child->materialOverride = "matA";
        Mc3ObjectState st;
        st.material = "matA";
        child->states["idle"] = st;
        root->children.push_back(child);
        doc.objects.push_back(root);

        auto def = makeObj("def_root");
        def->material = "matA";
        doc.definitions["defA"] = def;

        Mc3SceneState ss;
        ss.name = "sceneA";
        Mc3ObjectOverride ov;
        ov.id = "root";
        ov.material = "matA";
        ss.overrides.push_back(ov);
        doc.sceneStates["sceneA"] = ss;

        int cleared = clearMaterialReferencesAlg(doc, "matA");
        check(cleared == 5, "Materials: clears all 5 references (root.material, "
              "child.materialOverride, child.states[idle].material, "
              "def_root.material, sceneState override.material) -- got " +
              std::to_string(cleared));
        check(root->material.empty(), "Materials: root.material cleared");
        check(child->materialOverride.empty(), "Materials: child.materialOverride cleared");
        check(!child->states["idle"].material.has_value(), "Materials: state override cleared");
        check(def->material.empty(), "Materials: definition's own material cleared");
        check(!doc.sceneStates["sceneA"].overrides[0].material.has_value(),
              "Materials: scene-state override cleared");

        // A DIFFERENT material id must survive untouched.
        root->material = "matB";
        check(clearMaterialReferencesAlg(doc, "matA") == 0,
              "Materials: a second delete of the same (now-absent) id is a no-op");
        check(root->material == "matB", "Materials: an unrelated material id is untouched");
    }

    // --- Definitions: Instance.definition, variantDefinitions, and
    // assetMetadata.lods values, including inside a definition subtree. ---
    {
        Mc3Document doc;
        auto inst = makeObj("inst");
        inst->type = ObjectType::Instance;
        inst->definition = "defA";
        inst->variantDefinitions = {"defA", "defB", "defA"};
        doc.objects.push_back(inst);

        auto def = makeObj("defB_root");
        def->assetMetadata = Mc3AssetMetadata{};
        def->assetMetadata->lods["low"] = "defA";
        def->assetMetadata->lods["high"] = "defC";
        doc.definitions["defB"] = def;

        int cleared = clearDefinitionReferencesAlg(doc, "defA");
        check(inst->definition.empty(), "Definitions: Instance.definition cleared");
        check(inst->variantDefinitions.size() == 1 && inst->variantDefinitions[0] == "defB",
              "Definitions: both occurrences removed from variantDefinitions, unrelated entry kept");
        check(def->assetMetadata->lods.count("low") == 0,
              "Definitions: matching LOD tier removed");
        check(def->assetMetadata->lods.count("high") == 1,
              "Definitions: unrelated LOD tier kept");
        check(cleared == 4, "Definitions: reports 4 cleared references (1 Instance.definition + "
              "2 variantDefinitions occurrences + 1 LOD tier) -- got " + std::to_string(cleared));
    }

    // --- Textures: all 5 material texture-slot fields, across multiple materials. ---
    {
        Mc3Document doc;
        Mc3Material m1;
        m1.baseColorTexture = "texA";
        m1.normalTexture = "texA";
        Mc3Material m2;
        m2.emissiveTexture = "texA";
        m2.metallicRoughnessTexture = "texA";
        m2.occlusionTexture = "texA";
        m2.baseColorTexture = "texB"; // unrelated, must survive
        doc.materials["m1"] = m1;
        doc.materials["m2"] = m2;

        int cleared = clearTextureReferencesAlg(doc, "texA");
        check(cleared == 5, "Textures: clears all 5 references across both materials -- got " +
              std::to_string(cleared));
        check(doc.materials["m1"].baseColorTexture.empty() && doc.materials["m1"].normalTexture.empty(),
              "Textures: m1's two texA references cleared");
        check(doc.materials["m2"].emissiveTexture.empty() &&
              doc.materials["m2"].metallicRoughnessTexture.empty() &&
              doc.materials["m2"].occlusionTexture.empty(),
              "Textures: m2's three texA references cleared");
        check(doc.materials["m2"].baseColorTexture == "texB",
              "Textures: an unrelated texture id (texB) is untouched");
    }

    // --- Embeds: Mesh object meshSource "embed:<id>" scheme, not the bare id. ---
    {
        Mc3Document doc;
        auto mesh = makeObj("mesh1");
        mesh->meshSource = "embed:emb1";
        auto other = makeObj("mesh2");
        other->meshSource = "embed:emb2"; // unrelated, must survive
        doc.objects.push_back(mesh);
        doc.objects.push_back(other);

        int cleared = clearEmbedReferencesAlg(doc, "emb1");
        check(cleared == 1, "Embeds: clears exactly the one matching meshSource -- got " +
              std::to_string(cleared));
        check(mesh->meshSource.empty(), "Embeds: matching meshSource cleared");
        check(other->meshSource == "embed:emb2", "Embeds: unrelated meshSource untouched");
    }

    // --- Scripts: Mc3Object::scriptId + RunScript trigger steps removed. ---
    {
        Mc3Document doc;
        auto obj = makeObj("obj1");
        obj->scriptId = "scr1";
        doc.objects.push_back(obj);

        Mc3Trigger trig;
        trig.id = "t1";
        trig.steps.push_back({TriggerStepType::RunScript, "scr1"});
        trig.steps.push_back({TriggerStepType::RunScript, "scr2"});   // unrelated
        trig.steps.push_back({TriggerStepType::PlaySound, "scr1"});   // different TYPE, same ref text -- must survive
        doc.triggers["t1"] = trig;

        int cleared = clearScriptReferencesAlg(doc, "scr1");
        check(obj->scriptId.empty(), "Scripts: Mc3Object::scriptId cleared");
        check(doc.triggers["t1"].steps.size() == 2,
              "Scripts: only the matching RunScript step was removed (2 remain)");
        bool runScriptGone = true;
        for (auto& s : doc.triggers["t1"].steps)
            if (s.type == TriggerStepType::RunScript && s.ref == "scr1") runScriptGone = false;
        check(runScriptGone, "Scripts: the RunScript/scr1 step is gone");
        bool playSoundSurvived = false;
        for (auto& s : doc.triggers["t1"].steps)
            if (s.type == TriggerStepType::PlaySound && s.ref == "scr1") playSoundSurvived = true;
        check(playSoundSurvived,
              "Scripts: a PlaySound step with the same ref text (different type) is NOT touched");
        check(cleared == 2, "Scripts: reports 2 cleared references (scriptId + RunScript step) -- got " +
              std::to_string(cleared));
    }

    // --- Sounds / Music: PlaySound / PlayMusic trigger steps removed, cross-type isolation. ---
    {
        Mc3Document doc;
        Mc3Trigger trig;
        trig.id = "t1";
        trig.steps.push_back({TriggerStepType::PlaySound, "snd1"});
        trig.steps.push_back({TriggerStepType::PlayMusic, "snd1"}); // different type, same ref text
        trig.steps.push_back({TriggerStepType::PlaySound, "snd2"}); // unrelated
        doc.triggers["t1"] = trig;

        int cleared = clearSoundReferencesAlg(doc, "snd1");
        check(cleared == 1, "Sounds: clears only the matching PlaySound step -- got " +
              std::to_string(cleared));
        check(doc.triggers["t1"].steps.size() == 2, "Sounds: 2 steps remain");
        bool musicSurvived = false;
        for (auto& s : doc.triggers["t1"].steps)
            if (s.type == TriggerStepType::PlayMusic && s.ref == "snd1") musicSurvived = true;
        check(musicSurvived, "Sounds: a PlayMusic step with the same ref text is NOT touched");
    }
    {
        Mc3Document doc;
        Mc3Trigger trig;
        trig.id = "t1";
        trig.steps.push_back({TriggerStepType::PlayMusic, "trk1"});
        trig.steps.push_back({TriggerStepType::PlaySound, "trk1"}); // different type, same ref text
        doc.triggers["t1"] = trig;

        int cleared = clearMusicReferencesAlg(doc, "trk1");
        check(cleared == 1, "Music: clears only the matching PlayMusic step -- got " +
              std::to_string(cleared));
        check(doc.triggers["t1"].steps.size() == 1, "Music: 1 step remains");
        check(doc.triggers["t1"].steps[0].type == TriggerStepType::PlaySound,
              "Music: the PlaySound step with the same ref text is NOT touched");
    }

    if (failures == 0) { std::printf("All delete-reference-integrity tests passed.\n"); return 0; }
    std::fprintf(stderr, "%d delete-reference-integrity test(s) failed.\n", failures);
    return 1;
}
