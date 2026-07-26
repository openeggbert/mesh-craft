#include "MeshCraft/Application/MeshCraftApplication.hpp"
#include "MeshCraft/CoordinateSystemAlgorithms.hpp"
#include "MeshCraft/MeshCraftPrivate.hpp"

#include <Microsoft/Xna/Framework/Input/Keys.hpp>
#include <Microsoft/Xna/Framework/Input/KeyboardState.hpp>
#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

namespace MeshCraft::Application {

using namespace Microsoft::Xna::Framework::Input;
using namespace Microsoft::Xna::Framework;

namespace {

Matrix walkColliderWorldMatrix(const Mc3::Mc3Transform& t) {
    constexpr float radiansPerDegree = std::numbers::pi_v<float> / 180.0f;
    const float px = t.pivot[0], py = t.pivot[1], pz = t.pivot[2];
    return Matrix::CreateTranslation({-px, -py, -pz}) *
           Matrix::CreateScale({t.scale[0], t.scale[1], t.scale[2]}) *
           Matrix::CreateFromYawPitchRoll(t.rotation[1] * radiansPerDegree,
                                          t.rotation[0] * radiansPerDegree,
                                          t.rotation[2] * radiansPerDegree) *
           Matrix::CreateTranslation({t.position[0] + px, t.position[1] + py, t.position[2] + pz});
}

Matrix coordinateSystemRootMatrixForWalk(const Mc3::Mc3Document& doc) {
    return usesRightHandedZUpAlg(doc.coordinateSystem)
        ? Matrix::CreateRotationX(-std::numbers::pi_v<float> / 2.0f)
        : Matrix::getIdentityProperty();
}

std::optional<std::array<float, 3>> walkColliderHalfExtents(const Mc3::Mc3Object& obj) {
    if (!obj.primitive) return std::nullopt;
    const auto& p = *obj.primitive;
    const float radius = std::abs(p.radius);
    const float halfHeight = std::abs(p.height) * 0.5f;
    const float thin = 0.025f; // planes/disks still need a usable solid AABB
    switch (p.primitiveType) {
    case Mc3::PrimitiveType::Box:
    case Mc3::PrimitiveType::Cube:
        return std::array<float, 3>{std::abs(p.size[0]) * 0.5f,
                                    std::abs(p.size[1]) * 0.5f,
                                    std::abs(p.size[2]) * 0.5f};
    case Mc3::PrimitiveType::Sphere:
    case Mc3::PrimitiveType::IcoSphere:
        return std::array<float, 3>{radius, radius, radius};
    case Mc3::PrimitiveType::Cylinder:
    case Mc3::PrimitiveType::Cone:
    case Mc3::PrimitiveType::Capsule:
        if (p.axis == "x") return std::array<float, 3>{halfHeight, radius, radius};
        if (p.axis == "z") return std::array<float, 3>{radius, radius, halfHeight};
        return std::array<float, 3>{radius, halfHeight, radius};
    case Mc3::PrimitiveType::Plane:
    case Mc3::PrimitiveType::Grid:
        return std::array<float, 3>{std::abs(p.size[0]) * 0.5f, thin,
                                    std::abs(p.size[2]) * 0.5f};
    case Mc3::PrimitiveType::Disk:
        return std::array<float, 3>{radius, thin, radius};
    case Mc3::PrimitiveType::Torus: {
        const float outerRadius = std::abs(p.majorRadius) + std::abs(p.minorRadius);
        return std::array<float, 3>{outerRadius, std::abs(p.minorRadius), outerRadius};
    }
    }
    return std::nullopt;
}

float vectorLength(const Vector3& v) {
    return std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
}

bool finiteVector(const Vector3& v) {
    return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z);
}

Vector3 transformedDirection(const Vector3& local, const Matrix& world) {
    return Vector3::Transform(local, world) - Vector3::Transform(Vector3{}, world);
}

bool orthogonalScaledAxes(const Vector3& a, const Vector3& b, const Vector3& c) {
    const float al = vectorLength(a), bl = vectorLength(b), cl = vectorLength(c);
    constexpr float tolerance = 1.0e-3f;
    return al > tolerance && bl > tolerance && cl > tolerance &&
           std::abs(Vector3::Dot(a, b)) <= tolerance * al * bl &&
           std::abs(Vector3::Dot(a, c)) <= tolerance * al * cl &&
           std::abs(Vector3::Dot(b, c)) <= tolerance * bl * cl;
}

std::optional<Editor::WalkCollider> buildBoxWalkCollider(const Mc3::Mc3Object& obj,
                                                          const Matrix& world) {
    const auto extents = walkColliderHalfExtents(obj);
    if (!extents) return std::nullopt;
    Editor::WalkCollider collider;
    collider.minX = collider.minY = collider.minZ = std::numeric_limits<float>::infinity();
    collider.maxX = collider.maxY = collider.maxZ = -std::numeric_limits<float>::infinity();
    for (int sx : {-1, 1}) for (int sy : {-1, 1}) for (int sz : {-1, 1}) {
        const Vector3 p = Vector3::Transform(
            Vector3(sx * (*extents)[0], sy * (*extents)[1], sz * (*extents)[2]), world);
        collider.minX = std::min(collider.minX, p.X); collider.maxX = std::max(collider.maxX, p.X);
        collider.minY = std::min(collider.minY, p.Y); collider.maxY = std::max(collider.maxY, p.Y);
        collider.minZ = std::min(collider.minZ, p.Z); collider.maxZ = std::max(collider.maxZ, p.Z);
    }
    return collider;
}

std::optional<Editor::WalkCollider> buildSphereWalkCollider(const Mc3::Mc3Object& obj,
                                                             const Matrix& world) {
    if (!obj.primitive || (obj.primitive->primitiveType != Mc3::PrimitiveType::Sphere &&
                           obj.primitive->primitiveType != Mc3::PrimitiveType::IcoSphere))
        return std::nullopt;
    const Vector3 xAxis = transformedDirection({1.0f, 0.0f, 0.0f}, world);
    const Vector3 yAxis = transformedDirection({0.0f, 1.0f, 0.0f}, world);
    const Vector3 zAxis = transformedDirection({0.0f, 0.0f, 1.0f}, world);
    if (!finiteVector(xAxis) || !finiteVector(yAxis) || !finiteVector(zAxis) ||
        !orthogonalScaledAxes(xAxis, yAxis, zAxis))
        return std::nullopt;
    const float sx = vectorLength(xAxis), sy = vectorLength(yAxis), sz = vectorLength(zAxis);
    constexpr float tolerance = 1.0e-3f;
    if (std::abs(sx - sy) > tolerance * sx || std::abs(sx - sz) > tolerance * sx)
        return std::nullopt; // a non-uniformly scaled sphere is an ellipsoid, not a sphere proxy
    const Vector3 centre = Vector3::Transform(Vector3{}, world);
    const float radius = std::abs(obj.primitive->radius) * sx;
    if (!finiteVector(centre) || !std::isfinite(radius) || radius <= tolerance)
        return std::nullopt;
    return Editor::WalkCollider::sphere(centre.X, centre.Y, centre.Z, radius);
}

std::optional<Editor::WalkCollider> buildCapsuleWalkCollider(const Mc3::Mc3Object& obj,
                                                              const Matrix& world) {
    if (!obj.primitive || obj.primitive->primitiveType != Mc3::PrimitiveType::Capsule)
        return std::nullopt;
    const auto& p = *obj.primitive;
    Vector3 axis{0.0f, 1.0f, 0.0f};
    Vector3 radialA{1.0f, 0.0f, 0.0f};
    Vector3 radialB{0.0f, 0.0f, 1.0f};
    if (p.axis == "x") {
        axis = {1.0f, 0.0f, 0.0f}; radialA = {0.0f, 1.0f, 0.0f}; radialB = {0.0f, 0.0f, 1.0f};
    } else if (p.axis == "z") {
        axis = {0.0f, 0.0f, 1.0f}; radialA = {1.0f, 0.0f, 0.0f}; radialB = {0.0f, 1.0f, 0.0f};
    }
    const Vector3 worldAxis = transformedDirection(axis, world);
    const Vector3 worldRadialA = transformedDirection(radialA, world);
    const Vector3 worldRadialB = transformedDirection(radialB, world);
    if (!finiteVector(worldAxis) || !finiteVector(worldRadialA) || !finiteVector(worldRadialB) ||
        !orthogonalScaledAxes(worldAxis, worldRadialA, worldRadialB))
        return std::nullopt;
    const float axisScale = vectorLength(worldAxis);
    const float radialScaleA = vectorLength(worldRadialA);
    const float radialScaleB = vectorLength(worldRadialB);
    constexpr float tolerance = 1.0e-3f;
    if (std::abs(radialScaleA - radialScaleB) > tolerance * radialScaleA ||
        std::abs(worldAxis.X) > tolerance * axisScale ||
        std::abs(worldAxis.Z) > tolerance * axisScale)
        return std::nullopt; // tilted/elliptical capsules have no exact walk-mode meaning yet
    const float halfHeight = std::abs(p.height) * 0.5f;
    const Vector3 start = Vector3::Transform(axis * -halfHeight, world);
    const Vector3 end = Vector3::Transform(axis * halfHeight, world);
    const float radius = std::abs(p.radius) * radialScaleA;
    if (!finiteVector(start) || !finiteVector(end) || !std::isfinite(radius) || radius <= tolerance)
        return std::nullopt;
    return Editor::WalkCollider::capsule((start.X + end.X) * 0.5f,
                                         std::min(start.Y, end.Y), std::max(start.Y, end.Y),
                                         (start.Z + end.Z) * 0.5f, radius);
}

struct WalkColliderBuildReport {
    std::vector<Editor::WalkCollider> colliders;
    int unsupportedProxyCount{0};
    int budgetDroppedCount{0};
};

WalkColliderBuildReport buildWalkColliders(const Mc3::Mc3Document& doc) {
    WalkColliderBuildReport report;
    const Matrix identity = coordinateSystemRootMatrixForWalk(doc);
    std::function<void(const Mc3::Mc3Object&, const Matrix&, int)> visit;
    visit = [&](const Mc3::Mc3Object& obj, const Matrix& parentWorld, int depth) {
        if (depth > 16) return; // same graph-safety bound as SceneRenderer
        const Matrix world = walkColliderWorldMatrix(obj.transform) * parentWorld;
        if (!obj.collision.empty() && obj.collision != "none") {
            std::optional<Editor::WalkCollider> collider;
            if (obj.collision == "box")
                collider = buildBoxWalkCollider(obj, world);
            else if (obj.collision == "sphere")
                collider = buildSphereWalkCollider(obj, world);
            else if (obj.collision == "capsule")
                collider = buildCapsuleWalkCollider(obj, world);
            // mesh/convex (and unknown values) deliberately do not fall back
            // to a box: doing so would hide a changed collision contract from
            // the author.
            if (!collider) {
                ++report.unsupportedProxyCount;
            } else if (report.colliders.size() >= Editor::WalkController::maxCollisionProxies) {
                ++report.budgetDroppedCount;
            } else {
                report.colliders.push_back(*collider);
            }
        }
        for (const auto& child : obj.children)
            if (child) visit(*child, world, depth + 1);
    };
    for (const auto& obj : doc.objects)
        if (obj) visit(*obj, identity, 0);
    return report;
}

} // namespace

// SYS-W3-01 Phase 6: the movement/look physics and the view-matrix
// computation now live in the self-contained, CNA-coupled-where-
// unavoidable Editor::WalkController (self-contained like Preferences/
// KeybindingManager -- no callback DI needed, unlike MacroRecorder).
// These three methods are now thin wrappers translating walkController_'s
// results into Editor::EditorCamera state, plus the HUD drawing (which
// stays here as UI glue, matching the PropertiesPanel precedent of UI
// files calling into an extracted class rather than being extracted
// themselves).

void MeshCraftApplication::enterWalkMode() {
    auto report = buildWalkColliders(document_);
    walkColliders_ = std::move(report.colliders);
    walkUnsupportedProxyCount_ = report.unsupportedProxyCount;
    walkProxyBudgetDroppedCount_ = report.budgetDroppedCount;
    walkController_.enter(camera_.position(), camera_.yaw);
    std::string status = "Walk mode — " + std::to_string(walkColliders_.size()) +
                         " active collision proxy/proxies, Esc to exit";
    if (walkUnsupportedProxyCount_ > 0)
        status += "; " + std::to_string(walkUnsupportedProxyCount_) + " unsupported/incompatible ignored";
    if (walkProxyBudgetDroppedCount_ > 0)
        status += "; " + std::to_string(walkProxyBudgetDroppedCount_) + " over 256-proxy budget ignored";
    setStatusMsg(status, walkUnsupportedProxyCount_ > 0 || walkProxyBudgetDroppedCount_ > 0, 3.0f);
}

void MeshCraftApplication::exitWalkMode() {
    auto s = walkController_.exit();
    walkColliders_.clear();
    walkUnsupportedProxyCount_ = 0;
    walkProxyBudgetDroppedCount_ = 0;
    camera_.target   = s.target;
    camera_.yaw      = s.yaw;
    camera_.pitch    = s.pitch;
    camera_.distance = s.distance;
    setStatusMsg("Walk mode exited", false, 1.5f);
}

void MeshCraftApplication::updateWalkMode(float dt,
                                           const KeyboardState& ks,
                                           int mouseDx, int mouseDy)
{
    if (auto exitState = walkController_.update(dt, ks, mouseDx, mouseDy, walkColliders_)) {
        walkColliders_.clear();
        walkUnsupportedProxyCount_ = 0;
        walkProxyBudgetDroppedCount_ = 0;
        camera_.target   = exitState->target;
        camera_.yaw      = exitState->yaw;
        camera_.pitch    = exitState->pitch;
        camera_.distance = exitState->distance;
        setStatusMsg("Walk mode exited", false, 1.5f);
    }
}

void MeshCraftApplication::drawWalkModeHud(int screenW, int screenH) {
    // Crosshair at viewport centre
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float cx = static_cast<float>(screenW) * 0.5f;
    float cy = static_cast<float>(screenH) * 0.5f;
    const float arm = 10.0f;
    const ImU32 col = IM_COL32(255, 255, 255, 200);
    dl->AddLine(ImVec2(cx - arm, cy), ImVec2(cx + arm, cy), col, 1.5f);
    dl->AddLine(ImVec2(cx, cy - arm), ImVec2(cx, cy + arm), col, 1.5f);

    // Top-left info banner
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(kLeftPanelW) + 8, static_cast<float>(imguiTopH_) + 8),
                            ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("##walkbanner", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "WALK MODE");
    ImGui::Text("W/S  Forward/Back   |  A/D or Arrows  Turn");
    ImGui::Text("PgUp/PgDn or Mouse  Look   |  Ctrl  Jump   |  Esc  Exit");
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Pos: (%.1f, %.1f, %.1f)  Height: %.2f m",
                  walkController_.posX(), walkController_.posY(), walkController_.posZ(),
                  walkController_.height);
    ImGui::TextDisabled("%s", buf);
    ImGui::TextDisabled("Collision proxies: %d active (debug outlines shown)",
                        static_cast<int>(walkColliders_.size()));
    if (walkUnsupportedProxyCount_ > 0)
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f),
                           "%d unsupported/incompatible proxy/proxies ignored (mesh/convex are not approximated)",
                           walkUnsupportedProxyCount_);
    if (walkProxyBudgetDroppedCount_ > 0)
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f),
                           "%d proxy/proxies ignored after the 256-proxy budget",
                           walkProxyBudgetDroppedCount_);
    ImGui::End();

    // Settings popup (click to open)
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(screenW) - static_cast<float>(kRightPanelW) - 160.0f,
                                   static_cast<float>(imguiTopH_) + 8),
                            ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.70f);
    ImGui::Begin("##walksettings", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("Person height (m)");
    ImGui::SetNextItemWidth(140);
    // AlwaysClamp (AUDIT-0047): without it, Ctrl+Click text entry can set
    // these outside their slider bounds (incl. zero/negative), which would
    // break walk-mode movement/camera math with no other downstream guard.
    ImGui::SliderFloat("##wh", &walkController_.height, 0.5f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Text("Speed (m/s)");
    ImGui::SetNextItemWidth(140);
    ImGui::SliderFloat("##ws", &walkController_.speed, 1.0f, 20.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Text("Mouse sensitivity");
    ImGui::SetNextItemWidth(140);
    ImGui::SliderFloat("##wm", &walkController_.mouseSens, 0.001f, 0.010f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Text("Collision radius (m)");
    ImGui::SetNextItemWidth(140);
    ImGui::SliderFloat("##wcr", &walkController_.collisionRadius, 0.10f, 1.00f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::Button("Exit Walk Mode (Esc)"))
        exitWalkMode();
    ImGui::End();
}

} // namespace MeshCraft::Application
