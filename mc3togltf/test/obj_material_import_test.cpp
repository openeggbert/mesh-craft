// SYS-W14-35: material-aware OBJ parsing must preserve usemtl face groups,
// map the supported MTL PBR subset, warn on lossy fields, and reject hostile
// indices before any vertex/normal/UV dereference.

#include "MeshBuilder.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace mc3togltf;

static int failures = 0;

static void check(bool condition, const std::string& message) {
    if (condition) std::cout << "PASS: " << message << '\n';
    else { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

static const ObjMaterialGroup* findGroup(const ObjMaterialImportResult& result,
                                         const std::string& materialName) {
    for (const auto& group : result.groups)
        if (group.materialName == materialName) return &group;
    return nullptr;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <fixture-directory>\n";
        return 2;
    }
    const fs::path fixtures(argv[1]);

    try {
        const auto imported = importObjMaterialGroups({},
            (fixtures / "obj_material_groups.obj").string());
        check(imported.groups.size() == 2, "two usemtl groups become two MC3-import groups");
        const auto* red = findGroup(imported, "red_paint");
        const auto* blue = findGroup(imported, "blue_metal");
        check(red && red->materialIndex == 0 && red->mesh.indices.size() == 3,
              "red_paint owns exactly its source triangle");
        check(blue && blue->materialIndex == 1 && blue->mesh.indices.size() == 3,
              "blue_metal owns exactly its source triangle");
        if (red) {
            check(std::fabs(red->material.baseColor[0] - 0.8f) < 1e-5f &&
                  std::fabs(red->material.baseColor[3] - 0.75f) < 1e-5f &&
                  red->material.alphaMode == "blend",
                  "Kd/d maps to MC3 base color and blend alpha");
            check(std::fabs(red->material.roughness - 0.35f) < 1e-5f &&
                  std::fabs(red->material.metallic - 0.15f) < 1e-5f,
                  "Pr/Pm maps to MC3 roughness and metallic");
        }
        check(!imported.warnings.empty(), "unsupported MTL fields produce explicit import warnings");
    } catch (const std::exception& error) {
        check(false, std::string("multi-material fixture imports: ") + error.what());
    }

    try {
        const auto missing = importObjMaterialGroups({},
            (fixtures / "obj_missing_mtl.obj").string());
        check(missing.groups.size() == 1 && missing.groups.front().materialIndex == -1 &&
              missing.groups.front().mesh.indices.size() == 3,
              "missing MTL retains its safely parsed unassigned triangle");
        check(!missing.warnings.empty(), "missing MTL is surfaced as an import warning");
    } catch (const std::exception& error) {
        check(false, std::string("missing-MTL fixture remains importable: ") + error.what());
    }

    bool hostileRejected = false;
    try {
        (void)importObjMaterialGroups({}, (fixtures / "obj_hostile_material_index.obj").string());
    } catch (const std::runtime_error& error) {
        hostileRejected = std::string(error.what()).find("out of range") != std::string::npos;
    }
    check(hostileRejected, "hostile OBJ vertex index is rejected before dereference");
    check(parseObjMaterialIndex("-1").value_or(0) == -1 && parseObjMaterialIndex("1x") == std::nullopt,
          "persisted material selectors are parsed strictly");

    if (failures == 0) {
        std::cout << "All material-aware OBJ import checks passed.\n";
        return 0;
    }
    std::cerr << failures << " material-aware OBJ import check(s) failed.\n";
    return 1;
}
