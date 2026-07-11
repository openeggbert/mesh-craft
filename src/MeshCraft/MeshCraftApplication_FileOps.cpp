#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraftPrivate.hpp"
#include "MeshCraft/EditorAlgorithms.hpp"

#include "GltfExporter.hpp"
#include <tiny_gltf.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace MeshCraft {

void MeshCraftApplication::newScene() {
    document_ = Mc3::Mc3Document{};
    document_.model = "Untitled";
    selection_.clear();
    modified_ = false;
    currentFile_.clear();
    currentActionName_.clear();
    animTime_    = 0.0f;
    animPlaying_ = false;
    if (sceneRenderer_) {
        sceneRenderer_->setAnimOverrides({});
        // STAB-0250: without this, a stale CSG preview cache entry from the
        // previous document could collide (same content hash) with a CSG
        // node in the new scene and show the wrong cached geometry.
        sceneRenderer_->clearCsgCache();
    }
    std::cout << "[MeshCraft] New scene\n";
    updateWindowTitle();
}

std::filesystem::path MeshCraftApplication::autoSavePath(const std::filesystem::path& file) {
    return std::filesystem::path(autoSavePathAlg(file.string()));
}

void MeshCraftApplication::performAutoSave() {
    if (currentFile_.empty()) return;
    try {
        document_.saveToFile(autoSavePath(currentFile_));
        setStatusMsg("Auto-saved", false, 1.5f);
    } catch (const std::exception& e) {
        std::cerr << "[MeshCraft] Auto-save error: " << e.what() << "\n";
        setStatusMsg(std::string("Auto-save failed: ") + e.what(), true);
    }
}

void MeshCraftApplication::setStatusMsg(std::string msg, bool isError, float duration) {
    statusMsg_ = std::move(msg);
    statusMsgIsError_ = isError;
    statusMsgTimer_ = duration;
}

// STAB-0701: see header comment. Non-fatal -- the file still loads and
// displays/edits fine, only the *interpretation* of its rotation values
// during live rendering differs from what the file declares.
void MeshCraftApplication::checkRotationConventionNotice() {
    const bool nonDefaultUnits = document_.rotationUnits != "degrees";
    const bool nonDefaultOrder = document_.eulerOrder != "XYZ";
    if (!nonDefaultUnits && !nonDefaultOrder) return;

    std::string what = nonDefaultUnits && nonDefaultOrder
        ? "rotation_units=\"" + document_.rotationUnits + "\" and euler_order=\"" + document_.eulerOrder + "\""
        : nonDefaultUnits
            ? "rotation_units=\"" + document_.rotationUnits + "\""
            : "euler_order=\"" + document_.eulerOrder + "\"";
    setStatusMsg("Note: this file declares " + what +
                 " -- the editor's live preview always renders rotations as "
                 "degrees in XYZ order (export-only field, see STAB-0701)",
                 /*isError=*/false, /*duration=*/6.0f);
}


// AUD-031: loadRecentFiles/saveRecentFiles/addRecentFile were hand-copied
// duplicates of loadRecentFilesAlg/saveRecentFilesAlg/addRecentFileAlg's
// own logic (confirmed byte-identical); now delegate directly.
void MeshCraftApplication::loadRecentFiles() {
    loadRecentFilesAlg(recentFilesPath(), recentFiles_, kMaxRecentFiles);
}

void MeshCraftApplication::saveRecentFiles() {
    saveRecentFilesAlg(recentFilesPath(), recentFiles_);
}

void MeshCraftApplication::addRecentFile(const std::filesystem::path& path) {
    addRecentFileAlg(recentFiles_, path, kMaxRecentFiles);
    saveRecentFiles();
}

void MeshCraftApplication::confirmIfModified(PendingAction action, std::filesystem::path path) {
    pendingAction_   = action;
    pendingOpenPath_ = std::move(path);
    if (!modified_) executePendingAction();
    else            unsavedDlgOpen_ = true;
}

void MeshCraftApplication::executePendingAction() {
    switch (pendingAction_) {
    case PendingAction::NewScene:
        newScene();
        break;
    case PendingAction::OpenFile:
        openFile();
        break;
    case PendingAction::OpenRecentFile:
        if (!pendingOpenPath_.empty()) {
            try {
                document_ = Mc3::Mc3Document::loadFromFile(pendingOpenPath_);
                currentFile_ = pendingOpenPath_;
                addRecentFile(currentFile_);
                selection_.clear();
                undoStack_.clear(); redoStack_.clear();
                sceneRenderer_->clearCsgCache();
                modified_ = false;
                setStatusMsg("Opened " + currentFile_.filename().string(), false, 2.0f);
                checkRotationConventionNotice();
                updateWindowTitle();
            } catch (const std::exception& e) {
                std::cerr << "[MeshCraft] Open recent file error: " << e.what() << "\n";
                setStatusMsg(std::string("Failed to open ") +
                             pendingOpenPath_.filename().string() + ": " + e.what(), true);
            }
        }
        break;
    case PendingAction::ExitApp:
        Exit();
        break;
    default:
        break;
    }
    pendingAction_ = PendingAction::None;
}

void MeshCraftApplication::openFile() {
    openDialogBuf_[0] = '\0';
    openDialogErr_[0] = '\0';
    openDialogOpen_ = true;
}

void MeshCraftApplication::saveFile() {
    if (currentFile_.empty()) { saveFileAs(); return; }
    try {
        // F6: rotate backups before overwriting. AUD-031: was a hand-copied
        // duplicate of rotateBackupsAlg's own logic; now delegates to it.
        rotateBackupsAlg(currentFile_);
        document_.saveToFile(currentFile_);
        addRecentFile(currentFile_);
        modified_ = false;
        autoSaveCountdown_ = autoSaveInterval_ > 0.0f ? autoSaveInterval_ : 60.0f;
        { std::error_code ec; std::filesystem::remove(autoSavePath(currentFile_), ec); }
        std::cout << "[MeshCraft] Saved: " << currentFile_ << "\n";
        setStatusMsg("Saved " + currentFile_.filename().string(), false, 2.0f);
        updateWindowTitle();
    } catch (const std::exception& e) {
        std::cerr << "[MeshCraft] Save error: " << e.what() << "\n";
        setStatusMsg(std::string("Save error: ") + e.what(), true);
    }
}

void MeshCraftApplication::saveFileAs() {
    auto s = currentFile_.string();
    std::strncpy(saveDialogBuf_, s.c_str(), sizeof(saveDialogBuf_) - 1);
    saveDialogBuf_[sizeof(saveDialogBuf_) - 1] = '\0';
    saveDialogErr_[0] = '\0';
    saveDialogOpen_ = true;
}

void MeshCraftApplication::exportGltf() {
    if (currentFile_.empty()) {
        setStatusMsg("Export failed: save the file first", true);
        return;
    }
    // Derive default output path based on current format selection
    std::string outPath = currentFile_.string();
    const char* ext = (glbExportFmt_ == 1) ? ".gltf" : ".glb";
    auto pos = outPath.rfind(".mc3.xml");
    if (pos != std::string::npos) outPath.replace(pos, 8, ext);
    else outPath += ext;

    std::strncpy(glbExportOutBuf_, outPath.c_str(), sizeof(glbExportOutBuf_) - 1);
    glbExportOutBuf_[sizeof(glbExportOutBuf_) - 1] = '\0';
    glbExportErr_[0]  = '\0';
    glbExportOpen_    = true;
}

void MeshCraftApplication::exportObj() {
    if (currentFile_.empty()) {
        setStatusMsg("Export failed: save the file first", true);
        return;
    }
    std::string outPath = currentFile_.string();
    auto pos = outPath.rfind(".mc3.xml");
    if (pos != std::string::npos) outPath.replace(pos, 8, ".obj");
    else outPath += ".obj";

    std::strncpy(objExportOutBuf_, outPath.c_str(), sizeof(objExportOutBuf_) - 1);
    objExportOutBuf_[sizeof(objExportOutBuf_) - 1] = '\0';
    objExportErr_[0] = '\0';
    objExportOpen_   = true;
}

#ifdef __EMSCRIPTEN__
namespace {
// STAB-0571: an Emscripten build's "export" only ever writes into the
// in-browser-tab-only virtual MEMFS -- there's no way for the user to get
// the bytes onto their real filesystem otherwise. Reads the file back out
// of MEMFS and triggers a real browser download via a Blob + <a download>
// link (works in every browser, no permission prompt, unlike the
// Chromium-only File System Access API). Scoped to GLB only (see call
// site) since it downloads exactly one file -- a plain .gltf export
// writes a separate external .bin buffer (and would omit textures too)
// that a single-file download would silently leave behind.
EM_JS(void, meshcraftWebDownloadFile, (const char* path, const char* filename), {
    try {
        var data = FS.readFile(UTF8ToString(path));
        var blob = new Blob([data], {type: 'application/octet-stream'});
        var url = URL.createObjectURL(blob);
        var a = document.createElement('a');
        a.href = url;
        a.download = UTF8ToString(filename);
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        setTimeout(function() { URL.revokeObjectURL(url); }, 1000);
    } catch (e) {
        console.error('[MeshCraft] web download failed:', e);
    }
});
} // namespace
#endif

void MeshCraftApplication::runGltfExport(const std::string& outPath) {
    std::filesystem::path out(outPath);
    // outputFormatFromPath throws std::runtime_error for unknown extensions;
    // the export dialog catches it and displays it in glbExportErr_.
    mc3togltf::OutputFormat fmt = mc3togltf::outputFormatFromPath(out);

    std::cout << "[MeshCraft] Exporting to " << outPath << "\n";

    mc3togltf::GltfExporter exporter;
    exporter.allowApproximateCSG = glbAllowApproxCSG_;
    exporter.exportDocument(document_, out, fmt);

    const auto& s = exporter.stats;
    std::string statusMsg = "Exported " + out.filename().string()
        + " (" + std::to_string(s.uniqueMeshes) + " meshes"
        + (s.reusedMeshRefs > 0 ? ", " + std::to_string(s.reusedMeshRefs) + " reused" : "")
        + ")";

    // AUD-037: the exporter can drop/approximate geometry (e.g. an
    // approximate-CSG node exported as separate, geometrically-incorrect
    // meshes) while merely incrementing stats.warnings -- previously that
    // count only reached std::cout below, never the on-screen status, so
    // the editor showed an unqualified green "Exported" for a partial
    // export. Surface it and reuse the existing isError=true red styling
    // (already used elsewhere in this class for non-fatal cautions, not
    // just hard failures) so a warning-carrying export doesn't read as a
    // clean success.
    bool hasWarnings = s.warnings > 0;
    if (hasWarnings) {
        statusMsg += " — " + std::to_string(s.warnings)
            + (s.warnings == 1 ? " warning" : " warnings")
            + " (export may be incomplete or approximate; see console)";
    }

#ifdef __EMSCRIPTEN__
    if (fmt == mc3togltf::OutputFormat::GLB) {
        meshcraftWebDownloadFile(out.string().c_str(), out.filename().string().c_str());
        statusMsg += " — downloaded";
    } else {
        statusMsg += " — GLTF is multi-file, browser download not supported; use GLB";
    }
#endif

    setStatusMsg(statusMsg, hasWarnings);
    std::cout << "[MeshCraft] " << statusMsg
               << " — " << s.objectsProcessed << " objects, "
               << s.totalTriangles << " triangles, " << s.warnings << " warnings\n";
}

// ---------------------------------------------------------------------------
// STAB-0718: OBJ export
// ---------------------------------------------------------------------------

namespace {

// Column-major 4x4, matching glTF's own node.matrix convention.
struct Mat4 {
    double m[16];
    static Mat4 Identity() {
        Mat4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0;
        return r;
    }
    static Mat4 Multiply(const Mat4& a, const Mat4& b) {
        Mat4 r{};
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row) {
                double sum = 0.0;
                for (int k = 0; k < 4; ++k) sum += a.m[k * 4 + row] * b.m[col * 4 + k];
                r.m[col * 4 + row] = sum;
            }
        return r;
    }
    static Mat4 FromTRS(const std::vector<double>& t, const std::vector<double>& r, const std::vector<double>& s) {
        double tx = t.size() == 3 ? t[0] : 0.0, ty = t.size() == 3 ? t[1] : 0.0, tz = t.size() == 3 ? t[2] : 0.0;
        double qx = r.size() == 4 ? r[0] : 0.0, qy = r.size() == 4 ? r[1] : 0.0,
               qz = r.size() == 4 ? r[2] : 0.0, qw = r.size() == 4 ? r[3] : 1.0;
        double sx = s.size() == 3 ? s[0] : 1.0, sy = s.size() == 3 ? s[1] : 1.0, sz = s.size() == 3 ? s[2] : 1.0;
        double xx = qx*qx, yy = qy*qy, zz = qz*qz, xy = qx*qy, xz = qx*qz, yz = qy*qz, wx = qw*qx, wy = qw*qy, wz = qw*qz;
        Mat4 r4{};
        r4.m[0]  = (1.0 - 2.0*(yy+zz)) * sx; r4.m[1]  = (2.0*(xy+wz)) * sx;       r4.m[2]  = (2.0*(xz-wy)) * sx;       r4.m[3]  = 0.0;
        r4.m[4]  = (2.0*(xy-wz)) * sy;       r4.m[5]  = (1.0 - 2.0*(xx+zz)) * sy; r4.m[6]  = (2.0*(yz+wx)) * sy;       r4.m[7]  = 0.0;
        r4.m[8]  = (2.0*(xz+wy)) * sz;       r4.m[9]  = (2.0*(yz-wx)) * sz;       r4.m[10] = (1.0 - 2.0*(xx+yy)) * sz; r4.m[11] = 0.0;
        r4.m[12] = tx; r4.m[13] = ty; r4.m[14] = tz; r4.m[15] = 1.0;
        return r4;
    }
    // Transforms a point (w=1, translation applies).
    std::array<double,3> TransformPoint(const std::array<double,3>& p) const {
        return {
            m[0]*p[0] + m[4]*p[1] + m[8]*p[2]  + m[12],
            m[1]*p[0] + m[5]*p[1] + m[9]*p[2]  + m[13],
            m[2]*p[0] + m[6]*p[1] + m[10]*p[2] + m[14],
        };
    }
    // Transforms a direction (w=0, translation doesn't apply). Uses the
    // upper 3x3 directly rather than the inverse-transpose -- exact for
    // uniform scale/rotation/translation (the overwhelming common case for
    // mc3 scenes), a documented simplification for non-uniform scale
    // rather than the fully general (and costlier) normal matrix.
    std::array<double,3> TransformDirection(const std::array<double,3>& d) const {
        std::array<double,3> v{
            m[0]*d[0] + m[4]*d[1] + m[8]*d[2],
            m[1]*d[0] + m[5]*d[1] + m[9]*d[2],
            m[2]*d[0] + m[6]*d[1] + m[10]*d[2],
        };
        double len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
        if (len > 1e-12) { v[0] /= len; v[1] /= len; v[2] /= len; }
        return v;
    }
};

// AUD-003: buf.data is tinygltf's std::vector<unsigned char> -- no `float`/
// `uint16_t`/`uint32_t` object was ever created there, and byteOffset/stride
// are not guaranteed aligned to those types' requirements. Reading through a
// reinterpret_cast pointer is undefined behavior (type-punning through an
// incompatible type, plus a potentially-misaligned load); std::memcpy into a
// properly-typed local is well-defined and handles misalignment. This data is
// self-generated (a freshly-written GLB re-read in runObjExport), so nothing
// was observed to misbehave on mainstream x86/ARM -- but the construct is
// still non-portable UB, worth fixing at zero behavior change.
std::array<double,3> ReadVec3(const tinygltf::Model& model, const tinygltf::Accessor& acc, size_t i) {
    const auto& bv = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];
    size_t stride = bv.byteStride ? bv.byteStride : 12;
    size_t offset = bv.byteOffset + acc.byteOffset + i * stride;
    float f[3];
    std::memcpy(f, &buf.data[offset], sizeof(f));
    return { static_cast<double>(f[0]), static_cast<double>(f[1]), static_cast<double>(f[2]) };
}

std::array<double,2> ReadVec2(const tinygltf::Model& model, const tinygltf::Accessor& acc, size_t i) {
    const auto& bv = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];
    size_t stride = bv.byteStride ? bv.byteStride : 8;
    size_t offset = bv.byteOffset + acc.byteOffset + i * stride;
    float f[2];
    std::memcpy(f, &buf.data[offset], sizeof(f));
    return { static_cast<double>(f[0]), static_cast<double>(f[1]) };
}

uint32_t ReadIndex(const tinygltf::Model& model, const tinygltf::Accessor& acc, size_t i) {
    const auto& bv = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];
    size_t offset = bv.byteOffset + acc.byteOffset;
    switch (acc.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        return buf.data[offset + i];
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        uint16_t v;
        std::memcpy(&v, &buf.data[offset + i * sizeof(v)], sizeof(v));
        return v;
    }
    default: {
        uint32_t v;
        std::memcpy(&v, &buf.data[offset + i * sizeof(v)], sizeof(v));
        return v;
    }
    }
}

// Recursively walks the node graph, writing every mesh primitive's geometry
// (world-transformed) as OBJ v/vn/vt/f lines. `nextIdx` is 1-based (OBJ
// convention) and shared/incremented across the whole file, since OBJ face
// indices refer to the file's global vertex list, not a per-object one.
void WriteObjNode(const tinygltf::Model& model, int nodeIdx, const Mat4& parent,
                  std::ostream& obj, size_t& nextIdx, int& lastMaterial)
{
    const auto& node = model.nodes[nodeIdx];
    Mat4 local = !node.matrix.empty()
        ? Mat4{ { node.matrix[0],node.matrix[1],node.matrix[2],node.matrix[3],
                  node.matrix[4],node.matrix[5],node.matrix[6],node.matrix[7],
                  node.matrix[8],node.matrix[9],node.matrix[10],node.matrix[11],
                  node.matrix[12],node.matrix[13],node.matrix[14],node.matrix[15] } }
        : Mat4::FromTRS(node.translation, node.rotation, node.scale);
    Mat4 world = Mat4::Multiply(parent, local);

    if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size())) {
        const auto& mesh = model.meshes[node.mesh];
        obj << "o " << (node.name.empty() ? ("node" + std::to_string(nodeIdx)) : node.name) << "\n";
        for (const auto& prim : mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue; // OBJ export covers triangle meshes only
            if (prim.material != lastMaterial && prim.material >= 0 &&
                prim.material < static_cast<int>(model.materials.size())) {
                obj << "usemtl mat" << prim.material << "\n";
                lastMaterial = prim.material;
            }

            auto posIt = prim.attributes.find("POSITION");
            if (posIt == prim.attributes.end() || prim.indices < 0) continue;
            const auto& posAcc = model.accessors[posIt->second];
            auto normIt = prim.attributes.find("NORMAL");
            auto uvIt   = prim.attributes.find("TEXCOORD_0");
            const auto& idxAcc = model.accessors[prim.indices];

            size_t base = nextIdx;
            for (size_t i = 0; i < static_cast<size_t>(posAcc.count); ++i) {
                auto p = world.TransformPoint(ReadVec3(model, posAcc, i));
                obj << "v " << p[0] << " " << p[1] << " " << p[2] << "\n";
                if (normIt != prim.attributes.end()) {
                    auto n = world.TransformDirection(ReadVec3(model, model.accessors[normIt->second], i));
                    obj << "vn " << n[0] << " " << n[1] << " " << n[2] << "\n";
                }
                if (uvIt != prim.attributes.end()) {
                    auto uv = ReadVec2(model, model.accessors[uvIt->second], i);
                    obj << "vt " << uv[0] << " " << (1.0 - uv[1]) << "\n"; // glTF V is top-down, OBJ is bottom-up
                }
            }
            bool hasN = normIt != prim.attributes.end();
            bool hasT = uvIt != prim.attributes.end();
            for (size_t i = 0; i + 2 < static_cast<size_t>(idxAcc.count); i += 3) {
                obj << "f";
                for (int k = 0; k < 3; ++k) {
                    uint32_t vi = base + ReadIndex(model, idxAcc, i + k);
                    obj << " " << vi << (hasT ? "/" + std::to_string(vi) : (hasN ? "/" : ""))
                        << (hasN ? "/" + std::to_string(vi) : "");
                }
                obj << "\n";
            }
            nextIdx += posAcc.count;
        }
    }

    for (int child : node.children) WriteObjNode(model, child, world, obj, nextIdx, lastMaterial);
}

} // namespace

void MeshCraftApplication::runObjExport(const std::string& outPath) {
    std::filesystem::path out(outPath);
    std::filesystem::path tempGlb = std::filesystem::temp_directory_path() /
        ("meshcraft_objexport_" + std::to_string(reinterpret_cast<uintptr_t>(this)) + ".glb");

    mc3togltf::GltfExporter exporter;
    exporter.allowApproximateCSG = glbAllowApproxCSG_;
    exporter.exportDocument(document_, tempGlb, mc3togltf::OutputFormat::GLB);

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, tempGlb.string());
    std::error_code ec;
    std::filesystem::remove(tempGlb, ec);
    if (!ok)
        throw std::runtime_error("Failed to re-read intermediate GLB for OBJ export: " + err);

    std::ofstream obj(out);
    if (!obj)
        throw std::runtime_error("Failed to open " + out.string() + " for writing");
    obj << "# Exported from MeshCraft via OBJ export (STAB-0718)\n";
    if (!model.materials.empty()) {
        std::filesystem::path mtlPath = out;
        mtlPath.replace_extension(".mtl");
        obj << "mtllib " << mtlPath.filename().string() << "\n";
    }

    size_t nextIdx = 1;
    int lastMaterial = -1;
    if (model.defaultScene >= 0 && model.defaultScene < static_cast<int>(model.scenes.size())) {
        for (int rootIdx : model.scenes[model.defaultScene].nodes)
            WriteObjNode(model, rootIdx, Mat4::Identity(), obj, nextIdx, lastMaterial);
    }
    obj.close();

    if (!model.materials.empty()) {
        std::filesystem::path mtlPath = out;
        mtlPath.replace_extension(".mtl");
        std::ofstream mtl(mtlPath);
        for (size_t i = 0; i < model.materials.size(); ++i) {
            const auto& mat = model.materials[i];
            const auto& bc = mat.pbrMetallicRoughness.baseColorFactor; // 4 doubles, glTF default {1,1,1,1}
            mtl << "newmtl mat" << i << "\n"
                << "Kd " << bc[0] << " " << bc[1] << " " << bc[2] << "\n"
                << "d "  << bc[3] << "\n\n";
        }
    }

    setStatusMsg("Exported " + out.filename().string() + " (" +
                 std::to_string(nextIdx - 1) + " vertices)", false, 3.0f);
    std::cout << "[MeshCraft] Exported OBJ: " << outPath << " (" << (nextIdx - 1) << " vertices)\n";
}

// ---------------------------------------------------------------------------
// Edit operations
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// F3: Export selection to MC3 XML
// ---------------------------------------------------------------------------
void MeshCraftApplication::exportSelectionToFile(const std::string& path) {
    if (path.empty() || !selection_.hasSelection()) return;

    Mc3::Mc3Document tmp;

    // Collect material + texture keys referenced in an object subtree
    std::set<std::string> matKeys;
    std::function<void(const Mc3::Mc3Object&)> collectMats =
        [&](const Mc3::Mc3Object& obj) {
            if (!obj.material.empty())         matKeys.insert(obj.material);
            if (!obj.materialOverride.empty()) matKeys.insert(obj.materialOverride);
            for (const auto& c : obj.children) collectMats(*c);
        };

    for (const auto& sel : selection_.selection()) {
        tmp.objects.push_back(deepCopyObjectAlg(*sel));
        collectMats(*sel);
    }

    // Copy referenced materials
    std::set<std::string> texKeys;
    for (const auto& key : matKeys) {
        auto it = document_.materials.find(key);
        if (it == document_.materials.end()) continue;
        tmp.materials[key] = it->second;
        const auto& m = it->second;
        for (const auto& tk : { m.baseColorTexture, m.normalTexture,
                                 m.emissiveTexture, m.metallicRoughnessTexture,
                                 m.occlusionTexture })
            if (!tk.empty()) texKeys.insert(tk);
    }

    // Copy referenced textures
    for (const auto& key : texKeys) {
        auto it = document_.textures.find(key);
        if (it != document_.textures.end()) tmp.textures[key] = it->second;
    }

    tmp.saveToFile(path);
    setStatusMsg("Exported " + std::to_string(selection_.selection().size()) +
                 " object(s) → " + path, false, 3.0f);
}

// ---------------------------------------------------------------------------
// F4: Merge scene from MC3 XML
// ---------------------------------------------------------------------------

void MeshCraftApplication::mergeSceneFromFile(const std::string& path) {
    Mc3::Mc3Document src = Mc3::Mc3Document::loadFromFile(path);
    pushUndo();
    // AUD-031: was a hand-copied duplicate of mergeDocumentsAlg (textures/
    // materials/objects-with-id-collision-resolution/actions) plus its own
    // local collectObjectIds()/resolveObjectIdCollisions() duplicating
    // collectObjectIdsAlg()/resolveObjectIdCollisionsAlg() -- the comment
    // claiming this .cpp "can't include the headless-testable Alg header"
    // because it's CNA-coupled was simply wrong (EditorAlgorithms.hpp is
    // CNA-free; this file already includes it for loadPrefs()/savePrefs(),
    // AUD-031/AUD-032). Delegates to the single tested implementation now.
    int added = mergeDocumentsAlg(document_, src);
    modified_ = true; updateWindowTitle();
    setStatusMsg("Merged " + std::to_string(added) + " object(s) from " + path, false, 3.0f);
}

// ---------------------------------------------------------------------------
// H7: Preferences — load / save / apply theme
// ---------------------------------------------------------------------------
void MeshCraftApplication::applyTheme() {
    switch (prefTheme_) {
    case 1:  ImGui::StyleColorsLight();   break;
    case 2:  ImGui::StyleColorsClassic(); break;
    default: ImGui::StyleColorsDark();    break;
    }
}

// AUD-031: previously a separate hand-copied implementation of
// loadPrefsAlg/savePrefsAlg (EditorAlgorithms.hpp) with zero production
// call sites -- the two had already drifted (snapTranslate/snapScale
// default values differed, see PrefsAlg's own AUD-031 comment) since
// nothing forced them to stay in sync. Now delegates to the *Alg
// functions so there is exactly one tested implementation of the parse/
// clamp/serialize logic; seeding `p` from the live members before the
// call preserves "a missing file / an unparsed key leaves that field
// untouched" exactly as before (loadPrefsAlg only overwrites the keys it
// actually finds and successfully parses).
void MeshCraftApplication::loadPrefs() {
    PrefsAlg p;
    p.autoSaveInterval = autoSaveInterval_;
    p.snapTranslate    = snapTranslate_;
    p.snapRotate       = snapRotate_;
    p.snapScale        = snapScale_;
    p.gridSpacing       = gridSpacing_;
    p.theme             = prefTheme_;
    loadPrefsAlg(prefsPath(), p);
    autoSaveInterval_ = p.autoSaveInterval;
    snapTranslate_    = p.snapTranslate;
    snapRotate_       = p.snapRotate;
    snapScale_        = p.snapScale;
    gridSpacing_      = p.gridSpacing;
    prefTheme_        = p.theme;
    applyTheme();
}

void MeshCraftApplication::savePrefs() {
    auto path = prefsPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    PrefsAlg p;
    p.autoSaveInterval = autoSaveInterval_;
    p.snapTranslate    = snapTranslate_;
    p.snapRotate       = snapRotate_;
    p.snapScale        = snapScale_;
    p.gridSpacing      = gridSpacing_;
    p.theme            = prefTheme_;
    savePrefsAlg(path, p);
}

} // namespace MeshCraft
