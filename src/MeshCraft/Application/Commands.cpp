#include "MeshCraft/Application/MeshCraftApplication.hpp"
#include "MeshCraft/MeshCraftPrivate.hpp"
#include "MeshCraft/EditorAlgorithms.hpp"
#include "MeshCraft/LibraryWorkflowAlgorithms.hpp"
#include "GltfImporter.hpp"
#include "MeshBuilder.hpp"

#include <imgui.h>

// SYS-W14-03: declarations only -- deliberately NOT defining
// STB_IMAGE_WRITE_IMPLEMENTATION here. GltfExporter.cpp (mc3togltf_lib,
// which MeshCraft already links) already does that exactly once for the
// whole program; a second definition here would be a duplicate-symbol
// link error. mc3togltf_lib exposes tinygltf's vendored source directory
// as a SYSTEM PUBLIC include dir, so this header is already reachable
// with no new CMakeLists.txt dependency.
#include <stb_image_write.h>

#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
#include <Microsoft/Xna/Framework/Graphics/Viewport.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <numbers>
#include <random>
#include <string>

namespace MeshCraft::Application {

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;
using namespace Microsoft::Xna::Framework::Graphics;

void MeshCraftApplication::addPrimitive(Mc3::ObjectType type) {
    pushUndo();
    auto obj = std::make_shared<Mc3::Mc3Object>();
    obj->type = type;

    static int counter = 0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Object%d", ++counter);
    obj->name = buf;
    obj->id   = buf;

    switch (type) {
    case Mc3::ObjectType::Box:      { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Box;      p.size = {1.0f,1.0f,1.0f}; obj->primitive = p; } break;
    case Mc3::ObjectType::Sphere:   { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Sphere;   p.radius = 0.5f;            obj->primitive = p; } break;
    case Mc3::ObjectType::Cylinder: { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Cylinder; p.radius = 0.5f; p.height = 1.0f; obj->primitive = p; } break;
    case Mc3::ObjectType::Cone:     { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Cone;     p.radius = 0.5f; p.height = 1.0f; obj->primitive = p; } break;
    case Mc3::ObjectType::Plane:    { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Plane;    p.size = {1.0f,0.0f,1.0f};  obj->primitive = p; } break;
    case Mc3::ObjectType::Torus:    { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Torus; p.majorRadius = 0.35f; p.minorRadius = 0.15f; p.segments = 32; obj->primitive = p; } break;
    case Mc3::ObjectType::Capsule:  { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Capsule; p.radius = 0.5f; p.height = 1.0f; p.segments = 16; obj->primitive = p; } break;
    case Mc3::ObjectType::Disk:     { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Disk; p.radius = 0.5f; p.minorRadius = 0.0f; p.segments = 32; obj->primitive = p; } break;
    case Mc3::ObjectType::Grid:      { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Grid; p.size = {1.0f,0.0f,1.0f}; p.subdivisionsX = 4; p.subdivisionsZ = 4; obj->primitive = p; } break;
    case Mc3::ObjectType::IcoSphere: { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::IcoSphere; p.radius = 0.5f; obj->primitive = p; } break;
    // STAB-0721: give freshly-created Areas an editable size, matching what
    // areaType already gets when loaded from XML with a size= attribute
    // (Mc3XmlParser.cpp defaults primitiveType to Box for Area since it has
    // no ObjectType::Area case of its own) -- without this the Properties
    // panel's "Area (trigger zone)" label had nothing to show underneath it.
    case Mc3::ObjectType::Area:     { Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Box;      p.size = {1.0f,1.0f,1.0f}; obj->primitive = p; } break;
    case Mc3::ObjectType::Extrude: {
        Mc3::Mc3Extrude ex;
        ex.crossSection.type   = Mc3::CrossSectionType::Rect;
        ex.crossSection.width  = 0.3f;
        ex.crossSection.height = 0.3f;
        ex.path.type   = Mc3::ExtrudePathType::Line;
        ex.path.length = 1.0f;
        ex.path.axis   = "y";
        ex.segments    = 8;
        ex.smooth      = true;
        ex.caps        = true;
        obj->extrude = ex;
        break;
    }
    case Mc3::ObjectType::Union: {
        Mc3::Mc3CsgOperation csg; csg.csgType = Mc3::CsgType::Union;
        obj->csgOperation = csg; break;
    }
    case Mc3::ObjectType::Difference: {
        Mc3::Mc3CsgOperation csg; csg.csgType = Mc3::CsgType::Difference;
        obj->csgOperation = csg; break;
    }
    case Mc3::ObjectType::Intersection: {
        Mc3::Mc3CsgOperation csg; csg.csgType = Mc3::CsgType::Intersection;
        obj->csgOperation = csg; break;
    }
    default: break; // Group, Mesh, Instance — no primitive
    }
    obj->transform.position = { camera_.target.X, camera_.target.Y + 0.5f, camera_.target.Z };

    if (selection_.hasSelection()) {
        auto& sel0 = selection_.selection().front();
        bool isGroup = sel0->type == Mc3::ObjectType::Group   ||
                       sel0->type == Mc3::ObjectType::Union   ||
                       sel0->type == Mc3::ObjectType::Difference ||
                       sel0->type == Mc3::ObjectType::Intersection ||
                       !sel0->children.empty();
        if (isGroup) {
            sel0->children.push_back(obj);
            selection_.clear(); selection_.select(obj);
            modified_ = true;
            updateWindowTitle();
            macroRecorder_.recordStep("add", {objectTypeName(type)});
            return;
        }
    }
    document_.objects.push_back(obj);
    selection_.clear(); selection_.select(obj);
    modified_ = true;
    updateWindowTitle();
    macroRecorder_.recordStep("add", {objectTypeName(type)});
}

bool MeshCraftApplication::importObjWithMaterials(const std::string& path, std::string& error) {
    error.clear();
    if (path.empty()) {
        error = "Choose an OBJ file first.";
        return false;
    }

    mc3togltf::ObjMaterialImportResult imported;
    try {
        // The explicit editor import is permitted to read the selected source.
        // Persisted/exported documents remain subject to GltfExporter's strict
        // resource-path policy below: sources outside document_.sourcePath are
        // deliberately retained as absolute paths and rejected unless the
        // caller opts into external resources.
        imported = mc3togltf::importObjMaterialGroups(document_.sourcePath, path);
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
    if (imported.groups.empty()) {
        error = "OBJ contains no triangulated faces.";
        return false;
    }

    auto safeToken = [](std::string value, const std::string& fallback) {
        for (char& ch : value) {
            const unsigned char byte = static_cast<unsigned char>(ch);
            if (!std::isalnum(byte) && ch != '_' && ch != '-') ch = '_';
        }
        while (!value.empty() && value.back() == '_') value.pop_back();
        return value.empty() ? fallback : value;
    };
    const std::filesystem::path inputPath(path);
    const std::string sourceStem = safeToken(inputPath.stem().string(), "obj");

    // MC3 stores resources relative to its document directory. Preserve that
    // form only when the selected file is actually inside the document root;
    // never manufacture a `..` path to make an external OBJ look trusted.
    std::filesystem::path resolvedInput = inputPath;
    if (resolvedInput.is_relative() && !document_.sourcePath.empty())
        resolvedInput = document_.sourcePath / resolvedInput;
    std::error_code filesystemError;
    const std::filesystem::path absoluteInput = std::filesystem::absolute(resolvedInput, filesystemError);
    if (filesystemError) {
        error = "Cannot resolve OBJ path: " + filesystemError.message();
        return false;
    }
    std::filesystem::path storedSource = absoluteInput;
    if (!document_.sourcePath.empty()) {
        filesystemError.clear();
        const std::filesystem::path absoluteRoot =
            std::filesystem::absolute(document_.sourcePath, filesystemError);
        if (!filesystemError) {
            const std::filesystem::path relative =
                std::filesystem::relative(absoluteInput, absoluteRoot, filesystemError);
            if (!filesystemError && !relative.empty() && !relative.is_absolute() &&
                *relative.begin() != "..") {
                storedSource = relative.lexically_normal();
            }
        }
    }

    // Parsing/validation above completed without changing the document. The
    // material registry and object tree below are one undoable operation.
    pushUndo();
    auto importedRoot = std::make_shared<Mc3::Mc3Object>();
    importedRoot->type = Mc3::ObjectType::Group;
    importedRoot->name = sourceStem;
    importedRoot->transform.position = {camera_.target.X, camera_.target.Y + 0.5f, camera_.target.Z};

    int materialGroups = 0;
    for (const auto& group : imported.groups) {
        const std::string groupToken = safeToken(group.materialName,
                                                 group.materialIndex < 0 ? "unassigned" : "material");
        auto child = Mc3::Mc3Object::makeMesh(sourceStem + "_" + groupToken,
                                              storedSource.generic_string());
        child->metadata[std::string(mc3togltf::kObjMaterialIndexMetadataKey)] =
            std::to_string(group.materialIndex);
        if (!group.material.name.empty()) {
            std::string materialId = sourceStem + "_" + groupToken;
            int suffix = 2;
            while (document_.materials.count(materialId))
                materialId = sourceStem + "_" + groupToken + "_" + std::to_string(suffix++);
            Mc3::Mc3Material material = group.material;
            material.name = materialId;
            document_.materials[materialId] = std::move(material);
            child->material = materialId;
        }
        importedRoot->children.push_back(std::move(child));
        ++materialGroups;
    }

    std::set<std::string> assignedIds;
    regenerateSubtreeIdsAlg(*importedRoot, document_.objects, assignedIds);
    if (selection_.hasSelection()) {
        const auto& destination = selection_.selection().front();
        const bool acceptsChildren = destination->type == Mc3::ObjectType::Group ||
                                     destination->type == Mc3::ObjectType::Union ||
                                     destination->type == Mc3::ObjectType::Difference ||
                                     destination->type == Mc3::ObjectType::Intersection ||
                                     !destination->children.empty();
        if (acceptsChildren) destination->children.push_back(importedRoot);
        else document_.objects.push_back(importedRoot);
    } else {
        document_.objects.push_back(importedRoot);
    }
    selection_.clear();
    selection_.select(importedRoot);
    objectIndex_.invalidate();
    modified_ = true;
    updateWindowTitle();

    for (const std::string& warning : imported.warnings)
        std::cerr << "OBJ import warning: " << warning << '\n';
    std::string message = "Imported " + inputPath.filename().string() + " (" +
                          std::to_string(materialGroups) + " material group" +
                          (materialGroups == 1 ? ")" : "s)");
    if (!imported.warnings.empty())
        message += "; " + std::to_string(imported.warnings.size()) + " MTL property warning(s), see log";
    if (storedSource.is_absolute())
        message += "; external source remains blocked by safe export policy";
    setStatusMsg(message, false, 4.0f);
    return true;
}

bool MeshCraftApplication::importGltfWithTrustChoice(const std::string& path,
                                                      bool trustExternalGltf,
                                                      std::string& error) {
    error.clear();
    if (path.empty()) {
        error = "Choose a GLB file first.";
        return false;
    }

    mc3togltf::GltfImportResult imported;
    try {
        std::string extension = std::filesystem::path(path).extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if (extension == ".glb") {
            imported = mc3togltf::importSelfContainedGlb(path);
        } else if (extension == ".gltf" && trustExternalGltf) {
            imported = mc3togltf::importTrustedGltf(path);
        } else if (extension == ".gltf") {
            error = "External-resource .gltf needs the explicit Trusted import option.";
            return false;
        } else {
            error = "Choose a .glb file, or a .gltf file with Trusted import enabled.";
            return false;
        }
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
    if (imported.document.objects.empty() || imported.document.embeds.size() != 1) {
        error = "GLB contains no importable scene objects.";
        return false;
    }

    const std::filesystem::path inputPath(path);
    const std::string sourceStem = inputPath.stem().empty() ? "glb" : inputPath.stem().string();
    auto uniqueKey = [](const auto& registry, std::string base) {
        if (base.empty()) base = "import";
        std::string candidate = base;
        for (int suffix = 2; registry.count(candidate); ++suffix)
            candidate = base + "_" + std::to_string(suffix);
        return candidate;
    };

    // Validation above has finished without changing the current document.
    // Apply each imported registry after making every reference document-unique
    // so undo sees one coherent, reversible edit.
    pushUndo();
    const std::string sourceEmbedId = imported.document.embeds.begin()->first;
    const std::string embedId = uniqueKey(document_.embeds, sourceStem + "_source");
    document_.embeds[embedId] = std::move(imported.document.embeds.begin()->second);
    document_.embeds[embedId].id = embedId;

    std::map<std::string, std::string> textureIds;
    for (auto& [oldId, texture] : imported.document.textures) {
        const std::string newId = uniqueKey(document_.textures, sourceStem + "_" + oldId);
        texture.name = newId;
        document_.textures[newId] = std::move(texture);
        textureIds[oldId] = newId;
    }
    auto remapTexture = [&textureIds](std::string& id) {
        if (const auto found = textureIds.find(id); found != textureIds.end()) id = found->second;
    };
    std::map<std::string, std::string> materialIds;
    for (auto& [oldId, material] : imported.document.materials) {
        remapTexture(material.baseColorTexture);
        remapTexture(material.metallicRoughnessTexture);
        remapTexture(material.normalTexture);
        remapTexture(material.occlusionTexture);
        remapTexture(material.emissiveTexture);
        const std::string newId = uniqueKey(document_.materials, sourceStem + "_" + oldId);
        material.name = newId;
        document_.materials[newId] = std::move(material);
        materialIds[oldId] = newId;
    }
    std::function<void(Mc3::Mc3Object&)> remapObject = [&](Mc3::Mc3Object& object) {
        if (object.meshSource == "embed:" + sourceEmbedId)
            object.meshSource = "embed:" + embedId;
        if (const auto found = materialIds.find(object.material); found != materialIds.end())
            object.material = found->second;
        for (auto& child : object.children)
            if (child) remapObject(*child);
    };

    auto importedRoot = Mc3::Mc3Object::makeGroup(sourceStem);
    importedRoot->children = std::move(imported.document.objects);
    remapObject(*importedRoot);
    std::set<std::string> assignedIds;
    regenerateSubtreeIdsAlg(*importedRoot, document_.objects, assignedIds);
    if (selection_.hasSelection()) {
        const auto& destination = selection_.selection().front();
        const bool acceptsChildren = destination->type == Mc3::ObjectType::Group ||
                                     destination->type == Mc3::ObjectType::Union ||
                                     destination->type == Mc3::ObjectType::Difference ||
                                     destination->type == Mc3::ObjectType::Intersection ||
                                     !destination->children.empty();
        if (acceptsChildren) destination->children.push_back(importedRoot);
        else document_.objects.push_back(importedRoot);
    } else {
        document_.objects.push_back(importedRoot);
    }
    document_.cameras.insert(document_.cameras.end(),
                             std::make_move_iterator(imported.document.cameras.begin()),
                             std::make_move_iterator(imported.document.cameras.end()));
    document_.lights.insert(document_.lights.end(),
                            std::make_move_iterator(imported.document.lights.begin()),
                            std::make_move_iterator(imported.document.lights.end()));
    selection_.clear();
    selection_.select(importedRoot);
    objectIndex_.invalidate();
    modified_ = true;
    updateWindowTitle();

    for (const std::string& warning : imported.warnings)
        std::cerr << "GLB import warning: " << warning << '\n';
    std::string message = "Imported " + inputPath.filename().string() + " (" +
                          std::to_string(imported.triangleCount) + " triangles";
    if (!imported.warnings.empty())
        message += ", " + std::to_string(imported.warnings.size()) + " warning(s), see log";
    message += ")";
    setStatusMsg(message, false, 4.0f);
    return true;
}

bool MeshCraftApplication::importSelfContainedGlb(const std::string& path, std::string& error) {
    return importGltfWithTrustChoice(path, false, error);
}

void MeshCraftApplication::generateSimpleCollisionProxy() {
    const auto& selected = selection_.selection();
    if (selected.empty()) {
        setStatusMsg("Select a supported primitive first", true, 2.0f);
        return;
    }
    auto desiredProxy = [](const Mc3::Mc3Object& obj) -> const char* {
        if (!obj.primitive) return nullptr;
        switch (obj.primitive->primitiveType) {
        case Mc3::PrimitiveType::Box:
        case Mc3::PrimitiveType::Cube:
            return "box";
        case Mc3::PrimitiveType::Sphere:
        case Mc3::PrimitiveType::IcoSphere:
            return "sphere";
        case Mc3::PrimitiveType::Capsule:
            return "capsule";
        default:
            return nullptr;
        }
    };

    int changed = 0, supported = 0, unsupported = 0, locked = 0;
    for (const auto& object : selected) {
        if (objectLockState_.isLocked(object->id)) { ++locked; continue; }
        const char* proxy = desiredProxy(*object);
        if (!proxy) { ++unsupported; continue; }
        ++supported;
        if (object->collision != proxy) ++changed;
    }
    if (changed == 0) {
        std::string message = supported > 0
            ? "Simple collision proxies already assigned"
            : "No selected primitive supports a simple collision proxy";
        if (locked > 0) message += "; " + std::to_string(locked) + " locked skipped";
        if (unsupported > 0) message += "; " + std::to_string(unsupported) + " unsupported skipped";
        setStatusMsg(message, supported == 0, 2.5f);
        return;
    }

    pushUndo();
    for (const auto& object : selected) {
        if (objectLockState_.isLocked(object->id)) continue;
        if (const char* proxy = desiredProxy(*object)) object->collision = proxy;
    }
    modified_ = true;
    updateWindowTitle();
    std::string message = "Generated simple collision proxy for " + std::to_string(changed) + " object(s)";
    if (locked > 0) message += "; " + std::to_string(locked) + " locked skipped";
    if (unsupported > 0) message += "; " + std::to_string(unsupported) + " unsupported skipped";
    setStatusMsg(message, false, 2.5f);
}

// (removeFromList is defined in MeshCraftPrivate.hpp)

void MeshCraftApplication::deleteSelected() {
    if (!anySelectedUnlockedAlg(selection_.selection(), objectLockState_.ids())) return;
    pushUndo();
    for (const auto& s : selection_.selection()) {
        if (objectLockState_.isLocked(s->id)) continue;
        removeFromListAlg(document_.objects, s.get());
    }
    selection_.clear();
    modified_ = true;
    updateWindowTitle();
    macroRecorder_.recordStep("delete");
}
void MeshCraftApplication::toggleIsolate() {
    // SYS-W14-16: Mc3Object::visible is a real, persisted document field
    // (not editor-only state), and both branches below mutate it plus set
    // modified_ -- but neither called pushUndo(), so Ctrl+Z right after
    // isolating/un-isolating couldn't restore the pre-toggle visibility.
    // Found by a dedicated undo-coverage audit, not a quick pick.
    pushUndo();
    std::function<void(std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> walk;
    if (!isolateActive_) {
        // Activate: save visibility, hide non-selected
        preisolateVisibility_.clear();
        walk = [&](auto& list) {
            for (auto& o : list) {
                preisolateVisibility_[o->id] = o->visible;
                if (!selection_.isSelected(o.get())) o->visible = false;
                walk(o->children);
            }
        };
        walk(document_.objects);
        isolateActive_ = true;
        setStatusMsg("Isolation ON — Alt+I to exit", false, 2.5f);
    } else {
        // Deactivate: restore saved visibility
        walk = [&](auto& list) {
            for (auto& o : list) {
                auto it = preisolateVisibility_.find(o->id);
                if (it != preisolateVisibility_.end()) o->visible = it->second;
                walk(o->children);
            }
        };
        walk(document_.objects);
        preisolateVisibility_.clear();
        isolateActive_ = false;
        setStatusMsg("Isolation OFF", false, 1.5f);
    }
    modified_ = true;
    updateWindowTitle();
}

void MeshCraftApplication::saveCameraBookmark(int slot) {
    if (!cameraBookmarks_.save(slot, camera_)) return;
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Camera saved to slot %d", slot + 1);
    setStatusMsg(msg, false, 2.0f);
}

void MeshCraftApplication::restoreCameraBookmark(int slot) {
    if (!cameraBookmarks_.restore(slot, camera_))
        setStatusMsg("Slot is empty", true, 1.5f);
}

// (deepCopyObject and findParentList are defined in MeshCraftPrivate.hpp)

void MeshCraftApplication::duplicateSelected() {
    if (!selection_.hasSelection()) return;
    pushUndo();
    auto prev = selection_.selection();
    // AUD-031: was a hand-copied duplicate of duplicateObjectsAlg's own
    // document-mutation loop; now delegates to it directly (single tested
    // implementation).
    auto newObjs = duplicateObjectsAlg(document_.objects, prev);

    if (!newObjs.empty()) {
        selection_.clear();
        for (auto& o : newObjs) selection_.select(o);
        modified_ = true;
        updateWindowTitle();
        macroRecorder_.recordStep("duplicate");
    }
}

void MeshCraftApplication::copySelected() {
    if (!selection_.hasSelection()) return;
    clipboard_.clear();
    for (const auto& s : selection_.selection())
        clipboard_.push_back(deepCopyObjectAlg(*s));
}

void MeshCraftApplication::cutSelected() {
    if (!selection_.hasSelection()) return;
    copySelected();
    deleteSelected();
}

void MeshCraftApplication::pasteClipboard() {
    if (clipboard_.empty()) return;
    pushUndo();
    std::vector<std::shared_ptr<Mc3::Mc3Object>> newObjs;
    for (const auto& src : clipboard_) {
        auto copy = deepCopyObjectAlg(*src);
        copy->transform.position[0] += 1.0f;
        newObjs.push_back(copy);
    }
    if (selection_.hasSelection()) {
        auto& sel0 = selection_.selection().front();
        bool isGroup = sel0->type == Mc3::ObjectType::Group   ||
                       sel0->type == Mc3::ObjectType::Union   ||
                       sel0->type == Mc3::ObjectType::Difference ||
                       sel0->type == Mc3::ObjectType::Intersection ||
                       !sel0->children.empty();
        if (isGroup) {
            for (auto& o : newObjs) sel0->children.push_back(o);
            selection_.clear();
            for (auto& o : newObjs) selection_.select(o);
            modified_ = true;
            updateWindowTitle(); return;
        }
    }
    for (auto& o : newObjs) document_.objects.push_back(o);
    selection_.clear();
    for (auto& o : newObjs) selection_.select(o);
    modified_ = true;
    updateWindowTitle();
}

void MeshCraftApplication::groupSelected() {
    if (!selection_.hasSelection()) return;
    pushUndo();
    auto prev = selection_.selection();
    // Naming policy (a session-lifetime counter) stays here -- it's an
    // editor-session concern, not part of the document-mutation logic
    // groupObjectsAlg mirrors.
    static int groupCounter = 0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Group%d", ++groupCounter);
    // AUD-031: was a hand-copied duplicate of groupObjectsAlg's own
    // document-mutation logic; now delegates to it directly.
    auto group = groupObjectsAlg(document_.objects, prev, buf);
    selection_.clear(); selection_.select(group);
    modified_ = true;
    updateWindowTitle();
    macroRecorder_.recordStep("group");
}

void MeshCraftApplication::ungroupSelected() {
    if (!selection_.hasSelection()) return;
    auto& sel0 = selection_.selection().front();
    if (sel0->type != Mc3::ObjectType::Group || sel0->children.empty()) return;
    pushUndo();
    // AUD-031: was a hand-copied duplicate of ungroupObjectAlg's own
    // document-mutation logic; now delegates to it directly.
    auto children = ungroupObjectAlg(document_.objects, sel0);
    if (children.empty()) return;
    selection_.clear();
    for (auto& child : children) selection_.select(child);
    modified_ = true;
    updateWindowTitle();
    macroRecorder_.recordStep("ungroup");
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
std::vector<const Mc3::Mc3Object*> MeshCraftApplication::selectedPointers() const {
    std::vector<const Mc3::Mc3Object*> result;
    for (const auto& s : selection_.selection())
        result.push_back(s.get());
    return result;
}

void MeshCraftApplication::updateWindowTitle() {
    std::string title = "Mesh Craft";
    if (!document_.model.empty() && document_.model != "unnamed")
        title += " - " + document_.model;
    if (!currentFile_.empty())
        title += " [" + currentFile_.filename().string() + "]";
    if (modified_) title += " *";
    // Bounds-safe, exhaustive mapping (MeshCraft/Editor/ActiveTool.hpp).
    // Previously indexed a 9-element array with the 10-value ActiveTool enum,
    // so the Measure tool read one past the end (undefined behavior).
    title += " | "; title += activeToolName(activeTool_);
    getWindowProperty().setTitleProperty(title);
}

// ---------------------------------------------------------------------------
// ImGui UI
// ---------------------------------------------------------------------------
// SYS-W9-03: ids of the currently selected objects, in selection order.
// Objects with no id (empty string) can't be reliably re-found via
// flatFindSharedById() later -- matching flatFindById's own first-match-
// by-id semantics -- so they're skipped rather than recorded.
std::vector<std::string> MeshCraftApplication::currentSelectionIds() const {
    std::vector<std::string> ids;
    ids.reserve(selection_.selection().size());
    for (const auto& obj : selection_.selection())
        if (obj && !obj->id.empty()) ids.push_back(obj->id);
    return ids;
}

// SYS-W9-03: re-resolves each id against the CURRENT document_ (post-swap)
// and selects the objects found. An id that no longer exists post-swap
// (e.g. undone past the command that created it) is silently skipped --
// falls back toward an empty/partial selection rather than dangling or
// crashing, exactly as the human-authorized decision specified.
void MeshCraftApplication::restoreSelectionByIds(const std::vector<std::string>& ids) {
    selection_.clear();
    for (const auto& id : ids) {
        auto obj = flatFindSharedById(id);
        if (obj) selection_.select(obj);
    }
}

// SYS-W3-01 Phase 4: the stack push-then-trim-to-cap mechanism itself now
// lives in Editor::UndoManager (AUD-031's pushWithCapAlg moved with it).
// SYS-W9-04 additionally keeps an independent, memory-budgeted review
// timeline. The two snapshots must not share object pointers: a later
// in-place editor mutation must never alter either undo or checkpoint state.
void MeshCraftApplication::pushUndo() {
    auto selectionIds = currentSelectionIds();
    auto historySnapshot = deepCopyDoc(document_);
    undoManager_.push(deepCopyDoc(historySnapshot), selectionIds);
    const auto captured = sceneHistory_.capture(std::move(historySnapshot), std::move(selectionIds),
                                                "Before edit", Editor::SceneHistory::SnapshotKind::Automatic);
    if (!captured.stored) {
        historyNotice_ = captured.message;
    } else if (captured.evictedCount > 0) {
        historyNotice_ = "Automatic history evicted " + std::to_string(captured.evictedCount) +
                         " older snapshot(s) to stay within budget";
    } else {
        historyNotice_.clear();
    }
    // CSG cache no longer cleared here: hash-based invalidation handles it (K1)
    // SYS-W5-04: called before virtually every mutating command, so this is
    // objectIndex_'s single invalidation choke point for in-place tree edits.
    objectIndex_.invalidate();
}

void MeshCraftApplication::resetEventBindingSimulation() {
    eventSimulationEnabled_ = false;
    eventBindingRuntime_.reset();
    eventBindingSimulationReport_ = {};
}

bool MeshCraftApplication::undoOnActivate(bool widgetChanged) {
    if (ImGui::IsItemActivated()) pushUndo();
    return widgetChanged;
}

// Shared by the keyboard shortcut, the Edit menu, and the command palette
// (STAB-0486) so all 3 entry points stay behaviorally identical instead of
// hand-copied and free to drift.
//
// SYS-W9-03 (human-authorized decision, 2026-07-17): restores the selection
// that was active immediately before the command being undone/redone ran,
// instead of unconditionally clearing it -- UndoManager keeps the
// selection-id stacks in lockstep with the document stacks internally now
// (SYS-W3-01 Phase 4), returning both together as one Entry.
void MeshCraftApplication::performUndo() {
    auto entry = undoManager_.undo(deepCopyDoc(document_), currentSelectionIds());
    if (!entry) return;
    document_ = std::move(entry->doc);
    objectIndex_.invalidate();  // SYS-W5-04: wholesale document_ replacement
    resetEventBindingSimulation();
    restoreSelectionByIds(entry->selectionIds);
    modified_ = true;
    updateWindowTitle();
    evaluateAndPushAnimOverrides();
}

void MeshCraftApplication::performRedo() {
    auto entry = undoManager_.redo(deepCopyDoc(document_), currentSelectionIds());
    if (!entry) return;
    document_ = std::move(entry->doc);
    objectIndex_.invalidate();  // SYS-W5-04: wholesale document_ replacement
    resetEventBindingSimulation();
    restoreSelectionByIds(entry->selectionIds);
    modified_ = true;
    updateWindowTitle();
    evaluateAndPushAnimOverrides();
}

void MeshCraftApplication::captureSceneCheckpoint(const std::string& label) {
    const auto captured = sceneHistory_.capture(
        deepCopyDoc(document_), currentSelectionIds(), label, Editor::SceneHistory::SnapshotKind::Checkpoint);
    if (!captured.stored) {
        historyNotice_ = captured.message;
        setStatusMsg("Checkpoint was not stored: " + captured.message, true);
        return;
    }
    historyNotice_ = captured.evictedCount == 0 ? std::string{} :
        "Checkpoint storage evicted " + std::to_string(captured.evictedCount) + " older snapshot(s)";
    setStatusMsg("Stored checkpoint '" +
                 (label.empty() ? std::string("Checkpoint") : label) + "'");
}

void MeshCraftApplication::restoreSceneHistorySnapshot(Editor::SceneHistory::SnapshotId id) {
    const auto restored = sceneHistory_.restore(id);
    if (!restored) {
        setStatusMsg("History snapshot is no longer available", true);
        return;
    }
    // Restoring a review point is itself a document mutation. Preserve the
    // current scene in the unchanged exact undo stack first, so Ctrl+Z can
    // return from a checkpoint restore just like it can from every editor
    // command (and capture the same pre-restore state for review history).
    pushUndo();
    document_ = std::move(restored->doc);
    objectIndex_.invalidate();
    resetEventBindingSimulation();
    resetImportHealth();
    restoreSelectionByIds(restored->selectionIds);
    if (!document_.imports.empty()) resolveImports();
    modified_ = true;
    updateWindowTitle();
    evaluateAndPushAnimOverrides();
    if (importHealthError_.empty()) {
        setStatusMsg("Restored scene-history snapshot");
    } else {
        setStatusMsg("Restored scene-history snapshot, but import resolution needs attention: " +
                     importHealthError_, true);
    }
}

// ---------------------------------------------------------------------------
// Screenshot
// ---------------------------------------------------------------------------

bool MeshCraftApplication::saveScreenshot(const std::string& path) {
    auto& gd = getGraphicsDeviceProperty();
    int w = gd.getViewportProperty().getWidthProperty();
    int h = gd.getViewportProperty().getHeightProperty();
    if (w <= 0 || h <= 0) {
        std::cerr << "[Screenshot] failed: invalid viewport dimensions " << w << "x" << h << "\n";
        return false;
    }

    // AUD-083: GetBackBufferData() (backed by IGraphicsBackend::ReadBackbuffer
    // on every CNA backend) already synchronizes with the GPU and returns
    // pixels in XNA's top-to-bottom row order -- unlike raw glReadPixels(),
    // no manual glFinish()/row-flip is needed here.
    std::vector<Color> backBuffer(static_cast<size_t>(w) * h, Color(0, 0, 0, 0));
    gd.GetBackBufferData(backBuffer.data(), static_cast<int>(backBuffer.size()));

    std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 4);
    for (size_t i = 0; i < backBuffer.size(); ++i) {
        pixels[i * 4 + 0] = static_cast<unsigned char>(backBuffer[i].getRProperty());
        pixels[i * 4 + 1] = static_cast<unsigned char>(backBuffer[i].getGProperty());
        pixels[i * 4 + 2] = static_cast<unsigned char>(backBuffer[i].getBProperty());
        pixels[i * 4 + 3] = static_cast<unsigned char>(backBuffer[i].getAProperty());
    }

    // SYS-W14-03: --help/main.cpp's usage text has always promised "Render
    // scene to PNG and exit", but this unconditionally wrote raw PPM (P6)
    // bytes regardless of the requested extension -- a real path ending in
    // ".png" got PPM data with a misleading extension, not a decodable PNG.
    // A real PNG encoder (stbi_write_png(), from tinygltf's vendored
    // stb_image_write.h -- its implementation is already compiled into
    // mc3togltf_lib via GltfExporter.cpp's STB_IMAGE_WRITE_IMPLEMENTATION,
    // which MeshCraft already links, so this needs no new dependency) now
    // handles an explicit ".png" path; every other extension (in
    // particular every ".ppm" path this codebase's own test suite uses)
    // keeps writing the exact same raw PPM bytes as before, unchanged.
    std::string ext = std::filesystem::path(path).extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".png") {
        // pixels is already top-to-bottom (see above), so no flip is needed
        // before handing it to stb_image_write.
        if (stbi_write_png(path.c_str(), w, h, 4, pixels.data(), w * 4)) {
            std::cout << "[Screenshot] written " << path << "\n";
            return true;
        } else {
            std::cerr << "[Screenshot] failed to write PNG: " << path << "\n";
            return false;
        }
    }

    std::ofstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "[Screenshot] failed to open PPM for writing: " << path << "\n";
        return false;
    }
    f << "P6\n" << w << " " << h << "\n255\n";
    for (int row = 0; row < h; ++row)
        for (int col = 0; col < w; ++col) {
            int idx = (row * w + col) * 4;
            f.write(reinterpret_cast<char*>(&pixels[idx]), 3);
        }
    f.close();
    if (!f) {
        std::cerr << "[Screenshot] failed to write PPM: " << path << "\n";
        return false;
    }
    std::cout << "[Screenshot] written " << path << "\n";
    return true;
}

// ---------------------------------------------------------------------------
// Misc helpers
// ---------------------------------------------------------------------------

// SYS-W5-04: delegates to objectIndex_ (Editor::ObjectIndex), a lazily-
// rebuilt id/name -> object cache, instead of a fresh O(n) tree walk per
// call. Signature and first-match-in-document-order semantics unchanged --
// see ObjectIndex.hpp for the invalidation invariant this depends on.
Mc3::Mc3Object* MeshCraftApplication::flatFindById(const std::string& id) const {
    return objectIndex_.findById(document_, id);
}

std::shared_ptr<Mc3::Mc3Object> MeshCraftApplication::flatFindSharedById(const std::string& id) const {
    return objectIndex_.findSharedById(document_, id);
}

Mc3::Mc3Object* MeshCraftApplication::flatFindByName(const std::string& name) const {
    return objectIndex_.findByName(document_, name);
}

// ---------------------------------------------------------------------------
// Batch rename
// ---------------------------------------------------------------------------

void MeshCraftApplication::batchRenameSelected() {
    auto& sel = selection_.selection();
    if (sel.empty()) return;
    pushUndo();
    int renamed = batchRenameObjects(sel, objectLockState_.ids(), batchRenameBuf_, &document_.actions);
    modified_ = true;
    updateWindowTitle();
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Renamed %d object(s)", renamed);
    setStatusMsg(msg);
    macroRecorder_.recordStep("batch_rename", {batchRenameBuf_});
}

// ---------------------------------------------------------------------------
// Select parent
// ---------------------------------------------------------------------------

void MeshCraftApplication::selectParent() {
    if (!selection_.hasSelection()) return;
    const Mc3::Mc3Object* target = selection_.selection().front().get();
    Mc3::Mc3Object* parentRaw = findParentObject(document_.objects, target);
    if (!parentRaw) {
        setStatusMsg("Already at root level", false, 1.5f);
        return;
    }
    // Locate the shared_ptr that owns parentRaw so SelectionManager can hold it
    std::function<std::shared_ptr<Mc3::Mc3Object>(
        const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> findSptr;
    findSptr = [&](const auto& list) -> std::shared_ptr<Mc3::Mc3Object> {
        for (const auto& obj : list) {
            if (obj.get() == parentRaw) return obj;
            if (!obj->children.empty()) {
                auto r = findSptr(obj->children);
                if (r) return r;
            }
        }
        return nullptr;
    };
    auto parentSptr = findSptr(document_.objects);
    if (!parentSptr) return;
    selection_.clear();
    selection_.select(parentSptr);
    updateWindowTitle();
    setStatusMsg("Selected parent: " + parentSptr->name, false, 1.5f);
}

void MeshCraftApplication::breakInstance() {
    if (!selection_.hasSelection()) return;
    auto inst = selection_.selection().front();
    if (inst->type != Mc3::ObjectType::Instance) {
        setStatusMsg("Selected object is not an Instance", true, 2.0f);
        return;
    }
    if (!document_.definitions.count(inst->definition)) {
        setStatusMsg("Definition not found: " + inst->definition, true, 2.0f);
        return;
    }

    pushUndo();
    std::string definitionName = inst->definition;
    auto copy = breakInstanceAlg(document_, inst);

    selection_.clear();
    selection_.select(copy);
    modified_ = true; updateWindowTitle();
    setStatusMsg("Instance broken: " + definitionName, false, 2.0f);
}

void MeshCraftApplication::convertToDefinition() {
    if (!selection_.hasSelection()) return;
    auto src = selection_.selection().front();

    pushUndo();
    auto inst = convertToDefinitionAlg(document_, src);

    selection_.clear();
    selection_.select(inst);
    modified_ = true; updateWindowTitle();
    setStatusMsg("Converted to definition: " + inst->definition, false, 2.5f);
}

void MeshCraftApplication::createDefinitionFromSelection(const std::string& definitionId) {
    if (!selection_.hasSelection())
        throw std::invalid_argument("Select an object before creating a definition");

    pushUndo();
    auto inst = createNamedDefinitionFromSelectionAlg(
        document_, selection_.selection().front(), definitionId);
    selection_.clear();
    selection_.select(inst);
    modified_ = true;
    updateWindowTitle();
    setStatusMsg("Created definition " + definitionId + " from selection", false, 2.5f);
}

void MeshCraftApplication::publishDefinitionAsLibrary(const std::string& definitionId,
                                                       const std::string& libraryNamespace,
                                                       const std::string& version,
                                                       const std::filesystem::path& path) {
    Mc3::Mc3Document published = publishDefinitionAsLibraryAlg(
        document_, definitionId, libraryNamespace, version);
    const auto format = libraryFileFormatFromPathAlg(path);
    if (format == LibraryFileFormatAlg::Json)
        published.saveToLibraryJsonFile(path);
    else
        published.saveToLibraryFile(path);
    setStatusMsg("Published " + definitionId + " as " + path.filename().string() +
                 " (" + published.library->contentHash + ")", false, 3.0f);
}

void MeshCraftApplication::placeImportedDefinition(const std::string& definitionId) {
    if (!importedDefinitionKeys_.count(definitionId) || !document_.definitions.count(definitionId)) {
        setStatusMsg("Imported definition is no longer resolved: " + definitionId, true);
        return;
    }
    // addPrimitive owns the normal insertion/selection/undo behaviour and
    // places at the camera target, so the picker creates a regular editable
    // Instance rather than a special-case scene object.
    addPrimitive(Mc3::ObjectType::Instance);
    if (!selection_.hasSelection()) return;
    auto inst = selection_.selection().front();
    inst->definition = definitionId;
    inst->name = definitionId;
    setStatusMsg("Placed imported definition " + definitionId, false, 2.0f);
}

void MeshCraftApplication::exportSubtreeAsTemplate(const std::string& defName,
                                                   const std::string& filePath) {
    if (!selection_.hasSelection()) return;
    if (defName.empty()) return;

    auto src = selection_.selection().front();
    pushUndo();

    // Deep-copy subtree into definitions map (reset transform to identity)
    auto defObj = deepCopyObjectAlg(*src);
    defObj->id   = defName;
    defObj->name = defName;
    defObj->transform.position = {0.0f, 0.0f, 0.0f};
    defObj->transform.rotation = {0.0f, 0.0f, 0.0f};
    defObj->transform.scale    = {1.0f, 1.0f, 1.0f};
    // F8: if defName happens to collide with an existing <include>-sourced
    // definition id, this overwrite must win locally, not get silently
    // skipped by the writer on the next save because the old included
    // marker is still set for that id.
    document_.includedDefs.erase(defName);
    document_.definitions[defName] = defObj;

    // Optionally save the subtree to a file — carries along any materials/
    // textures the subtree references, so the standalone file doesn't
    // silently lose its appearance when loaded elsewhere (STAB-0536).
    if (!filePath.empty()) {
        Mc3::Mc3Document tmp = exportSubtreeTemplateAlg(document_, defName, defObj);
        tmp.saveToFile(filePath);
    }

    // Replace original with an Instance pointing to the new definition
    auto inst = std::make_shared<Mc3::Mc3Object>();
    inst->id         = src->id;
    inst->name       = src->name.empty() ? defName : src->name;
    inst->type       = Mc3::ObjectType::Instance;
    inst->definition = defName;
    inst->transform  = src->transform;
    inst->visible    = src->visible;
    inst->layer      = src->layer;
    inst->tags       = src->tags;

    auto* parentList = findParentListAlg(document_.objects, src.get());
    if (parentList) {
        for (auto& obj : *parentList) {
            if (obj.get() == src.get()) { obj = inst; break; }
        }
    } else {
        document_.objects.push_back(inst);
    }

    selection_.clear();
    selection_.select(inst);
    modified_ = true; updateWindowTitle();
    std::string msg = "Exported as template: " + defName;
    if (!filePath.empty()) msg += " → " + filePath;
    setStatusMsg(msg, false, 3.0f);
}

void MeshCraftApplication::alignToObject() {
    if (selection_.selection().size() < 2) return;
    const std::string& srcName = selection_.selection().front()->name;
    pushUndo();
    int aligned = alignToObjectAlg(selection_.selection(), objectLockState_.ids());
    modified_ = true; updateWindowTitle();
    setStatusMsg("Aligned " + std::to_string(aligned) +
                 " object(s) to " + srcName, false, 2.0f);
}

// Shared by the Edit menu and the command palette (STAB-0486) so both stay
// behaviorally identical instead of hand-copied and free to drift.
void MeshCraftApplication::dropSelectedToGroundPlane() {
    if (!selection_.hasSelection()) return;
    if (!anySelectedUnlockedAlg(selection_.selection(), objectLockState_.ids())) return;
    pushUndo();
    int dropped = 0;
    for (const auto& s : selection_.selection()) {
        if (objectLockState_.isLocked(s->id)) continue;
        float bottomOffset = 0.0f; // distance from pivot to lowest point
        if (s->primitive) {
            const auto& p = *s->primitive;
            float sy = std::abs(s->transform.scale[1]);
            switch (p.primitiveType) {
                case Mc3::PrimitiveType::Box:
                case Mc3::PrimitiveType::Cube:   bottomOffset = (p.size[1]*sy)/2.0f; break;
                case Mc3::PrimitiveType::Sphere: bottomOffset = p.radius*sy;         break;
                case Mc3::PrimitiveType::Cylinder:
                case Mc3::PrimitiveType::Cone:   bottomOffset = (p.height/2.0f)*sy; break;
                default: bottomOffset = 0.0f; break;
            }
        }
        s->transform.position[1] = bottomOffset;
        ++dropped;
    }
    modified_ = true; updateWindowTitle();
    setStatusMsg("Dropped " + std::to_string(dropped) + " object(s) to ground plane");
}

// H14: Scale selected objects around their group center
void MeshCraftApplication::groupScaleSelected() {
    const auto& sel = selection_.selection();
    if (sel.empty()) return;
    if (!anySelectedUnlockedAlg(sel, objectLockState_.ids())) return;
    const float f = groupScaleFactor_;

    pushUndo();
    int scaled = groupScaleAlg(sel, objectLockState_.ids(), f);
    modified_ = true; updateWindowTitle();
    setStatusMsg("Group scale ×" + std::to_string(f).substr(0, 5) +
                 " on " + std::to_string(scaled) + " object(s)", false, 2.0f);
    macroRecorder_.recordStep("group_scale", {std::to_string(f)});
}

void MeshCraftApplication::selectChildren() {
    if (!selection_.hasSelection()) return;
    const auto& sel0 = selection_.selection().front();
    if (sel0->children.empty()) {
        setStatusMsg("No children", false, 1.5f);
        return;
    }
    selection_.clear();
    for (auto& c : flattenDescendantsAlg(sel0->children)) selection_.select(c);
    updateWindowTitle();
    setStatusMsg("Selected " + std::to_string(selection_.selection().size()) + " child object(s)", false, 1.5f);
}

// ---------------------------------------------------------------------------
// Randomize Transform
// ---------------------------------------------------------------------------

void MeshCraftApplication::randomizeTransformSelected() {
    auto& sel = selection_.selection();
    if (sel.empty()) return;
    if (!anySelectedUnlockedAlg(sel, objectLockState_.ids())) return;
    pushUndo();
    std::mt19937 rng{std::random_device{}()};
    auto rand11 = [&]() -> float {
        return std::uniform_real_distribution<float>(-1.0f, 1.0f)(rng);
    };
    int count = 0;
    for (const auto& s : sel) {
        if (objectLockState_.isLocked(s->id)) continue;
        for (int i = 0; i < 3; ++i) {
            if (scatterPosRange_[i] != 0.0f)
                s->transform.position[i] += rand11() * scatterPosRange_[i];
            if (scatterRotRange_[i] != 0.0f)
                s->transform.rotation[i] += rand11() * scatterRotRange_[i];
        }
        if (scatterScaleRange_ != 0.0f) {
            float delta = rand11() * scatterScaleRange_ * 0.01f;
            for (int i = 0; i < 3; ++i)
                s->transform.scale[i] *= (1.0f + delta);
        }
        ++count;
    }
    modified_ = true;
    updateWindowTitle();
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Randomized %d object(s)", count);
    setStatusMsg(msg);
}

// ---------------------------------------------------------------------------
// Find & Replace names
// ---------------------------------------------------------------------------

void MeshCraftApplication::findReplaceNames() {
    const std::string findStr(findBuf_);
    const std::string replStr(replaceBuf_);
    if (findStr.empty()) return;

    // Build selected-ID set for selectedOnly mode
    std::set<std::string> selectedIds;
    if (findSelectedOnly_)
        for (const auto& s : selection_.selection()) selectedIds.insert(s->id);

    int matchCount = countFindReplaceMatches(document_.objects, objectLockState_.ids(),
                                             findStr, replStr, findCaseSensitive_,
                                             findSelectedOnly_, selectedIds);
    if (matchCount == 0) {
        setStatusMsg("No matches found for \"" + findStr + "\"", true, 2.5f);
        return;
    }

    pushUndo();
    applyFindReplaceNames(document_.objects, objectLockState_.ids(),
                          findStr, replStr, findCaseSensitive_,
                          findSelectedOnly_, selectedIds);
    modified_ = true;
    updateWindowTitle();
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Replaced %d object name(s)", matchCount);
    setStatusMsg(msg);
}

// ---------------------------------------------------------------------------
// Linear Array (array duplicate)
// ---------------------------------------------------------------------------

void MeshCraftApplication::arrayDuplicate() {
    if (!selection_.hasSelection()) return;
    auto prev = selection_.selection();
    pushUndo();

    auto newObjs = arrayDuplicateObjects(document_.objects, prev,
                                          arrayDupCount_, arrayDupAxis_,
                                          arrayDupSpacing_, arrayDupRelative_);
    if (!newObjs.empty()) {
        selection_.clear();
        for (const auto& src : prev) selection_.select(src);
        for (auto& o : newObjs) selection_.select(o);
        modified_ = true;
        updateWindowTitle();
        char msg[64];
        std::snprintf(msg, sizeof(msg), "Array: %d object(s) created",
                      static_cast<int>(newObjs.size()));
        setStatusMsg(msg);
        macroRecorder_.recordStep("linear_array", {std::to_string(std::max(2, arrayDupCount_)),
                                    std::to_string(std::clamp(arrayDupAxis_, 0, 2)),
                                    std::to_string(arrayDupSpacing_)});
    }
}

// ---------------------------------------------------------------------------
// H4: Scatter Along Curve
// ---------------------------------------------------------------------------
void MeshCraftApplication::scatterAlongCurve() {
    if (!selection_.hasSelection() || scatterCurveCount_ < 2) return;

    auto prev = selection_.selection();
    pushUndo();

    // Jitter seed per call
    unsigned seed = static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    auto rng = [&]() -> float {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed & 0xFFFF) / 65535.0f * 2.0f - 1.0f;
    };

    ScatterCurveParamsAlg params;
    params.count       = scatterCurveCount_;
    params.mode        = scatterCurveMode_;
    params.axis        = scatterCurveAxis_;
    params.spacing     = scatterCurveSpacing_;
    params.arcAngleDeg = scatterCurveArcAngle_;
    params.radius      = scatterCurveRadius_;
    params.jitter      = scatterCurveJitter_;

    auto newObjs = scatterAlongCurveAlg(document_.objects, prev, params, rng);

    if (!newObjs.empty()) {
        selection_.clear();
        for (const auto& s : prev) selection_.select(s);
        for (auto& o : newObjs) selection_.select(o);
        modified_ = true; updateWindowTitle();
        setStatusMsg("Scattered " + std::to_string(newObjs.size()) + " copies along curve");
    }
}

// ---------------------------------------------------------------------------
// Copy Properties to Selected
// ---------------------------------------------------------------------------

void MeshCraftApplication::copyPropsToSelected() {
    const auto& sel = selection_.selection();
    if (sel.size() < 2) { setStatusMsg("Need 2+ objects selected", true, 2.0f); return; }

    const auto& src = *sel.front();
    pushUndo();
    int count = 0;
    for (int i = 1; i < static_cast<int>(sel.size()); ++i) {
        auto& dst = *sel[i];
        if (copyPropsMaterial_)         dst.material         = src.material;
        if (copyPropsMaterialOverride_) dst.materialOverride = src.materialOverride;
        if (copyPropsCollision_)        dst.collision        = src.collision;
        if (copyPropsTags_)             dst.tags             = src.tags;
        if (copyPropsVisibility_)       dst.visible          = src.visible;
        if (copyPropsIsCutter_)         dst.isCutter         = src.isCutter;
        ++count;
    }
    modified_ = true;
    updateWindowTitle();
    char msg[80];
    std::snprintf(msg, sizeof(msg), "Copied properties to %d object(s)", count);
    setStatusMsg(msg);
}

// ---------------------------------------------------------------------------
// Animation helpers
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// H5 — Reset pivot to (0,0,0) while keeping geometry in world space
// ---------------------------------------------------------------------------

void MeshCraftApplication::resetPivot() {
    using namespace Microsoft::Xna::Framework;
    if (!selection_.hasSelection()) return;
    if (!anySelectedUnlockedAlg(selection_.selection(), objectLockState_.ids())) return;
    pushUndo();
    const float deg = std::numbers::pi_v<float> / 180.0f;
    for (const auto& s : selection_.selection()) {
        if (objectLockState_.isLocked(s->id)) continue;
        const auto& p = s->transform.pivot;
        const auto& rot = s->transform.rotation;
        // Compensation: pos_new = pos + d*R - d  where d = -pivot (zeroing pivot)
        float dX = -p[0], dY = -p[1], dZ = -p[2];
        Matrix R = Matrix::CreateFromYawPitchRoll(rot[1]*deg, rot[0]*deg, rot[2]*deg);
        float rX = dX*R.M11 + dY*R.M21 + dZ*R.M31;
        float rY = dX*R.M12 + dY*R.M22 + dZ*R.M32;
        float rZ = dX*R.M13 + dY*R.M23 + dZ*R.M33;
        s->transform.position[0] += rX - dX;
        s->transform.position[1] += rY - dY;
        s->transform.position[2] += rZ - dZ;
        s->transform.pivot = {0.0f, 0.0f, 0.0f};
    }
    modified_ = true;
    updateWindowTitle();
    setStatusMsg("Pivot reset to origin", false, 1.5f);
}

} // namespace MeshCraft::Application
