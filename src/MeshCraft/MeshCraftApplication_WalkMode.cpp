#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraftPrivate.hpp"

#include <Microsoft/Xna/Framework/Input/Keys.hpp>
#include <Microsoft/Xna/Framework/Input/KeyboardState.hpp>
#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>

#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <numbers>
#include <optional>

#include <imgui.h>

namespace MeshCraft {

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

std::vector<Editor::WalkCollider> buildWalkColliders(const Mc3::Mc3Document& doc) {
    std::vector<Editor::WalkCollider> colliders;
    const Matrix identity = Matrix::getIdentityProperty();
    std::function<void(const Mc3::Mc3Object&, const Matrix&, int)> visit;
    visit = [&](const Mc3::Mc3Object& obj, const Matrix& parentWorld, int depth) {
        if (depth > 16) return; // same graph-safety bound as SceneRenderer
        const Matrix world = walkColliderWorldMatrix(obj.transform) * parentWorld;
        if (obj.collision == "box") {
            if (const auto extents = walkColliderHalfExtents(obj)) {
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
                colliders.push_back(collider);
            }
        }
        for (const auto& child : obj.children)
            if (child) visit(*child, world, depth + 1);
    };
    for (const auto& obj : doc.objects)
        if (obj) visit(*obj, identity, 0);
    return colliders;
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
    walkColliders_ = buildWalkColliders(document_);
    walkController_.enter(camera_.position(), camera_.yaw);
    setStatusMsg("Walk mode — " + std::to_string(walkColliders_.size()) +
                 " box collider(s), Esc to exit", false, 3.0f);
}

void MeshCraftApplication::exitWalkMode() {
    auto s = walkController_.exit();
    walkColliders_.clear();
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

} // namespace MeshCraft
