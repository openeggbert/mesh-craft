// AUD-064 follow-up: Mc3JsonParser.cpp read segments/sides/subdivisions
// fields with a raw get<int>() and applied none of the per-field clamps
// (kMaxTessellation == 4096) the XML loader has enforced since AUD-005 --
// so a hand-edited or AI-generated .mc3.json bypassed that hardening
// entirely, including AUD-064's own Grid subdivisions_x*subdivisions_z
// case. This mirrors input_budget_test.cpp's XML coverage for the JSON
// load path.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

static const Mc3Object* findByName(const Mc3Document& doc, const std::string& n) {
    for (const auto& o : doc.objects) if (o && o->name == n) return o.get();
    return nullptr;
}

int main() {
    const int kMaxTess = 4096;  // must match Mc3JsonParser.cpp's own kMaxTessellation

    // Hostile counts must be clamped, not passed through -- same fixture
    // shape as input_budget_test.cpp's XML case, translated to mc3.json.
    {
        const std::string js = R"json(
        {
          "version": "0.3",
          "model": "budget",
          "objects": [
            { "id": "1", "name": "huge", "type": "sphere",
              "primitive": { "primitiveType": "sphere", "radius": 1, "segments": 100000000 } },
            { "id": "2", "name": "grid", "type": "grid",
              "primitive": { "primitiveType": "grid", "subdivisionsX": 999999, "subdivisionsZ": 888888 } },
            { "id": "3", "name": "neg", "type": "sphere",
              "primitive": { "primitiveType": "sphere", "radius": 1, "segments": -5 } }
          ]
        }
        )json";
        Mc3Document doc = Mc3Document::loadFromJsonString(js);

        const Mc3Object* huge = findByName(doc, "huge");
        check(huge && huge->primitive, "huge sphere parsed");
        if (huge && huge->primitive)
            check(huge->primitive->segments <= kMaxTess,
                  "segments=100000000 clamped to <= " + std::to_string(kMaxTess));

        const Mc3Object* grid = findByName(doc, "grid");
        check(grid && grid->primitive, "grid parsed");
        if (grid && grid->primitive) {
            check(grid->primitive->subdivisionsX <= kMaxTess,
                  "subdivisionsX clamped to <= " + std::to_string(kMaxTess));
            check(grid->primitive->subdivisionsZ <= kMaxTess,
                  "subdivisionsZ clamped to <= " + std::to_string(kMaxTess));
        }

        const Mc3Object* neg = findByName(doc, "neg");
        check(neg && neg->primitive, "neg sphere parsed");
        if (neg && neg->primitive)
            check(neg->primitive->segments >= 0, "negative segments clamped to >= 0");
    }

    // Legitimate values must survive unchanged.
    {
        const std::string js = R"json(
        {
          "version": "0.3", "model": "ok",
          "objects": [
            { "id": "1", "name": "ok", "type": "sphere",
              "primitive": { "primitiveType": "sphere", "radius": 1, "segments": 48 } }
          ]
        }
        )json";
        Mc3Document doc = Mc3Document::loadFromJsonString(js);
        const Mc3Object* ok = findByName(doc, "ok");
        check(ok && ok->primitive, "ok sphere parsed");
        if (ok && ok->primitive)
            check(ok->primitive->segments == 48, "legitimate segments=48 preserved");
    }

    // Extrude cross-section sides/segments must also be clamped.
    {
        const std::string js = R"json(
        {
          "version": "0.3", "model": "extrude-budget",
          "objects": [
            { "id": "1", "name": "ext", "type": "extrude",
              "extrude": {
                "crossSection": { "type": "circle", "radius": 1, "sides": 99999999, "segments": 99999999 },
                "path": { "type": "line", "length": 1 },
                "segments": 99999999
              } }
          ]
        }
        )json";
        Mc3Document doc = Mc3Document::loadFromJsonString(js);
        const Mc3Object* ext = findByName(doc, "ext");
        check(ext && ext->extrude, "extrude object parsed");
        if (ext && ext->extrude) {
            check(ext->extrude->crossSection.sides <= kMaxTess,
                  "crossSection.sides clamped to <= " + std::to_string(kMaxTess));
            check(ext->extrude->crossSection.segments <= kMaxTess,
                  "crossSection.segments clamped to <= " + std::to_string(kMaxTess));
            check(ext->extrude->segments <= kMaxTess,
                  "extrude.segments clamped to <= " + std::to_string(kMaxTess));
        }
    }

    if (failures == 0) { std::cout << "All JSON input-budget tests passed.\n"; return 0; }
    std::cerr << failures << " JSON input-budget test(s) failed.\n";
    return 1;
}
