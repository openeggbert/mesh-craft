#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraftPrivate.hpp"
#include "MeshCraft/Scene/SceneHierarchyPanel.hpp"

#include <imgui.h>

#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
#include <Microsoft/Xna/Framework/Graphics/Viewport.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <set>
#include <string>

namespace MeshCraft {

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;
using namespace Microsoft::Xna::Framework::Graphics;


void MeshCraftApplication::drawLeftPanel(float panelY, float panelH)
{
    int tlPanelH = showTimeline_ ? kTimelineH : 0;
    (void)tlPanelH;
    // -----------------------------------------------------------------------
    // Left panel — tabbed (Scene / Lights)
    // -----------------------------------------------------------------------
    ImGui::SetNextWindowPos(ImVec2(0, panelY));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(kLeftPanelW), panelH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.155f, 0.155f, 0.155f, 1.0f));
    ImGui::Begin("##leftpanel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    if (ImGui::BeginTabBar("##lefttabs")) {

        // -------------------------------------------------------------------
        // Tab: Scene hierarchy  (L1 — delegated to SceneHierarchyPanel)
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Scene")) {
            Scene::HierarchyCallbacks cb;
            cb.pushUndo       = [this]() { pushUndo(); };
            cb.markModified   = [this]() { modified_ = true; updateWindowTitle(); };
            cb.duplicateSel   = [this]() { duplicateSelected(); };
            cb.deleteSel      = [this]() { deleteSelected(); };
            cb.selectParent   = [this]() { selectParent(); };
            cb.selectChildren = [this]() { selectChildren(); };
            cb.openBatchRename = [this]() { batchRenameOpen_ = true; };
            hierarchyPanel_->draw(selection_, lockedIds_, cb);
            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Lights
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Lights")) {
            // Clamp selection in case lights were deleted
            if (selectedLightIdx_ >= static_cast<int>(document_.lights.size()))
                selectedLightIdx_ = static_cast<int>(document_.lights.size()) - 1;

            // Toolbar: Add / Remove
            if (ImGui::SmallButton("+")) {
                pushUndo();
                Mc3::Mc3Light newLight;
                newLight.name = "Light " + std::to_string(document_.lights.size() + 1);
                document_.lights.push_back(newLight);
                selectedLightIdx_ = static_cast<int>(document_.lights.size()) - 1;
                modified_ = true; updateWindowTitle();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("-") && selectedLightIdx_ >= 0) {
                pushUndo();
                document_.lights.erase(document_.lights.begin() + selectedLightIdx_);
                selectedLightIdx_ = std::min(selectedLightIdx_,
                    static_cast<int>(document_.lights.size()) - 1);
                modified_ = true; updateWindowTitle();
            }

            // Light list
            ImGui::Separator();
            for (int i = 0; i < static_cast<int>(document_.lights.size()); ++i) {
                const auto& li = document_.lights[i];
                const char* typeStr =
                    li.type == Mc3::LightType::Ambient      ? "[Amb]" :
                    li.type == Mc3::LightType::Directional  ? "[Dir]" :
                    li.type == Mc3::LightType::Spot         ? "[Spt]" : "[Pnt]";
                std::string label = std::string(typeStr) + " " +
                    (li.name.empty() ? ("light_" + std::to_string(i)) : li.name);
                ImGui::PushID(i);
                if (ImGui::Selectable(label.c_str(), selectedLightIdx_ == i))
                    selectedLightIdx_ = i;
                ImGui::PopID();
            }

            // Inline editor for selected light
            if (selectedLightIdx_ >= 0 && selectedLightIdx_ < static_cast<int>(document_.lights.size())) {
                auto& li = document_.lights[selectedLightIdx_];
                ImGui::Separator();
                ImGui::Spacing();

                // Name
                {
                    char buf[128];
                    std::strncpy(buf, li.name.c_str(), sizeof(buf)-1); buf[127]='\0';
                    ImGui::TextDisabled("Name");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##lname", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); li.name = buf; modified_ = true; updateWindowTitle();
                    }
                }

                // Type
                {
                    const char* types[] = { "Ambient", "Directional", "Spot", "Point" };
                    int tidx = static_cast<int>(li.type);
                    ImGui::TextDisabled("Type");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##ltype", &tidx, types, 4)) {
                        pushUndo(); li.type = static_cast<Mc3::LightType>(tidx);
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Color
                // AUDIT-0059: these light-property widgets set modified_
                // (unsaved-changes indicator) without pushUndo(), unlike the
                // Name/Type fields above -- so the user sees "unsaved
                // changes" but Ctrl+Z can't revert the edit. Added pushUndo(),
                // gated on IsItemActivated() for continuous-drag widgets
                // (Drag/Slider/ColorEdit fire every frame during a drag, so
                // an unconditional pushUndo() would push one snapshot per
                // frame and capture intermediate values, not the pre-edit
                // state -- matches the established pattern already used
                // elsewhere in this file, e.g. the camera near/far fields).
                // Checkbox fires once per click, so it's safe unconditional.
                ImGui::TextDisabled("Color");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit3("##lcol", li.color.data(),
                        ImGuiColorEditFlags_NoLabel)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true; updateWindowTitle();
                }

                // Brightness
                ImGui::TextDisabled("Brightness");
                ImGui::SetNextItemWidth(-1);
                // AlwaysClamp (AUDIT-0043): bounded light/camera/fog params
                // below have no other downstream guard against an
                // out-of-range Ctrl+Click-typed value; unbounded ones
                // (position/rotation/direction DragFloat3 calls with no
                // explicit min/max) are deliberately left untouched.
                if (ImGui::DragFloat("##lbrt", &li.brightness, 0.01f, 0.0f, 100.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true; updateWindowTitle();
                }

                // Cast shadows
                if (ImGui::Checkbox("Cast Shadows", &li.castShadows)) {
                    pushUndo(); modified_ = true; updateWindowTitle();
                }

                // Direction (Directional / Spot)
                if (li.type == Mc3::LightType::Directional || li.type == Mc3::LightType::Spot) {
                    ImGui::TextDisabled("Direction");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat3("##ldir", li.direction.data(), 0.01f, -1.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Position (Spot / Point)
                if (li.type == Mc3::LightType::Spot || li.type == Mc3::LightType::Point) {
                    ImGui::TextDisabled("Position");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat3("##lpos", li.position.data(), 0.1f)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Range (0=unlimited)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##lrng", &li.range, 0.1f, 0.0f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Spot-only params
                if (li.type == Mc3::LightType::Spot) {
                    ImGui::TextDisabled("Angle (deg)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderFloat("##lang", &li.angle, 0.0f, 90.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }
                    ImGui::TextDisabled("Falloff");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderFloat("##lfal", &li.falloff, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Environment
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Env")) {
            // Enable / disable environment block
            bool envEnabled = document_.environment.has_value();
            if (ImGui::Checkbox("Enable", &envEnabled)) {
                pushUndo();
                if (envEnabled) document_.environment = Mc3::Mc3Environment{};
                else            document_.environment.reset();
                modified_ = true; updateWindowTitle();
            }

            if (document_.environment.has_value()) {
                auto& env = *document_.environment;

                ImGui::Spacing();

                // Background color
                // AUDIT-0059: missing pushUndo(), same class of gap as the
                // light editor above -- IsItemActivated()-gated to avoid a
                // per-frame push during a continuous color-picker drag.
                ImGui::TextDisabled("Background Color");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit3("##envbg", env.backgroundColor.data(),
                        ImGuiColorEditFlags_NoLabel)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true; updateWindowTitle();
                }

                // Background texture
                ImGui::TextDisabled("Background Texture");
                {
                    char buf[256];
                    std::strncpy(buf, env.backgroundTexture.c_str(), sizeof(buf)-1); buf[255]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##envbgtex", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); env.backgroundTexture = buf;
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Skybox texture (equirectangular panorama)
                ImGui::TextDisabled("Skybox Texture (equirect)");
                {
                    char buf[256];
                    std::strncpy(buf, env.skyboxTexture.c_str(), sizeof(buf)-1); buf[255]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##envskybox", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); env.skyboxTexture = buf;
                        modified_ = true; updateWindowTitle();
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Fog
                bool fogEnabled = env.fog.has_value();
                if (ImGui::Checkbox("Fog", &fogEnabled)) {
                    pushUndo();
                    if (fogEnabled) env.fog = Mc3::Mc3Fog{};
                    else            env.fog.reset();
                    modified_ = true; updateWindowTitle();
                }

                if (env.fog.has_value()) {
                    auto& fog = *env.fog;

                    ImGui::TextDisabled("Fog Color");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::ColorEdit3("##fogcol", fog.color.data(),
                            ImGuiColorEditFlags_NoLabel)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }

                    ImGui::TextDisabled("Mode");
                    ImGui::SetNextItemWidth(-1);
                    const char* fogModes[] = { "Linear", "Exponential" };
                    int fogModeIdx = (fog.mode == Mc3::FogMode::Exponential) ? 1 : 0;
                    if (ImGui::Combo("##fogmode", &fogModeIdx, fogModes, 2)) {
                        pushUndo();
                        fog.mode = fogModeIdx == 1 ? Mc3::FogMode::Exponential : Mc3::FogMode::Linear;
                        modified_ = true; updateWindowTitle();
                    }

                    if (fog.mode == Mc3::FogMode::Linear) {
                        ImGui::TextDisabled("Start");
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat("##fogstart", &fog.start, 0.5f, 0.0f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                            if (ImGui::IsItemActivated()) pushUndo();
                            modified_ = true; updateWindowTitle();
                        }
                        ImGui::TextDisabled("End");
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat("##fogend", &fog.end, 0.5f, 0.0f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                            if (ImGui::IsItemActivated()) pushUndo();
                            modified_ = true; updateWindowTitle();
                        }
                    } else {
                        ImGui::TextDisabled("Density");
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat("##fogdens", &fog.density, 0.001f, 0.0f, 1.0f, "%.4f", ImGuiSliderFlags_AlwaysClamp)) {
                            if (ImGui::IsItemActivated()) pushUndo();
                            modified_ = true; updateWindowTitle();
                        }
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Cameras
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Cam")) {
            if (selectedCameraIdx_ >= static_cast<int>(document_.cameras.size()))
                selectedCameraIdx_ = static_cast<int>(document_.cameras.size()) - 1;

            // Toolbar
            if (ImGui::SmallButton("+")) {
                pushUndo();
                Mc3::Mc3Camera cam;
                cam.name = "Camera " + std::to_string(document_.cameras.size() + 1);
                document_.cameras.push_back(cam);
                selectedCameraIdx_ = static_cast<int>(document_.cameras.size()) - 1;
                modified_ = true; updateWindowTitle();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("-") && selectedCameraIdx_ >= 0) {
                pushUndo();
                const std::string& removedName = document_.cameras[selectedCameraIdx_].name;
                if (document_.defaultCamera == removedName) document_.defaultCamera.clear();
                document_.cameras.erase(document_.cameras.begin() + selectedCameraIdx_);
                selectedCameraIdx_ = std::min(selectedCameraIdx_,
                    static_cast<int>(document_.cameras.size()) - 1);
                modified_ = true; updateWindowTitle();
            }

            // Camera list
            ImGui::Separator();
            for (int i = 0; i < static_cast<int>(document_.cameras.size()); ++i) {
                const auto& cam = document_.cameras[i];
                bool isDefault = (cam.name == document_.defaultCamera);
                std::string label = (isDefault ? "* " : "  ") +
                    (cam.name.empty() ? "camera_" + std::to_string(i) : cam.name) +
                    (cam.type == Mc3::CameraType::Orthographic ? " [Ort]" : " [Per]");
                ImGui::PushID(i);
                if (ImGui::Selectable(label.c_str(), selectedCameraIdx_ == i))
                    selectedCameraIdx_ = i;
                ImGui::PopID();
            }

            // Inline editor
            if (selectedCameraIdx_ >= 0 &&
                selectedCameraIdx_ < static_cast<int>(document_.cameras.size()))
            {
                auto& cam = document_.cameras[selectedCameraIdx_];
                ImGui::Separator();
                ImGui::Spacing();

                // Name
                {
                    char buf[128];
                    std::strncpy(buf, cam.name.c_str(), sizeof(buf)-1); buf[127]='\0';
                    ImGui::TextDisabled("Name");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##cname", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo();
                        if (document_.defaultCamera == cam.name) document_.defaultCamera = buf;
                        cam.name = buf;
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Default camera toggle
                {
                    bool isDefault = (cam.name == document_.defaultCamera);
                    if (ImGui::Checkbox("Default", &isDefault)) {
                        pushUndo();
                        document_.defaultCamera = isDefault ? cam.name : "";
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Type
                {
                    const char* types[] = { "Perspective", "Orthographic" };
                    int tidx = static_cast<int>(cam.type);
                    ImGui::TextDisabled("Type");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##ctype", &tidx, types, 2)) {
                        pushUndo();
                        cam.type = static_cast<Mc3::CameraType>(tidx);
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Position
                ImGui::TextDisabled("Position");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat3("##cpos", cam.position.data(), 0.1f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true; updateWindowTitle();
                }

                // Target
                ImGui::TextDisabled("Target");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat3("##ctgt", cam.target.data(), 0.1f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true; updateWindowTitle();
                }

                // Rotation override (optional)
                {
                    bool hasRot = cam.rotation.has_value();
                    if (ImGui::Checkbox("Override Rotation", &hasRot)) {
                        pushUndo();
                        if (hasRot) cam.rotation = std::array<float,3>{0.0f,0.0f,0.0f};
                        else        cam.rotation.reset();
                        modified_ = true; updateWindowTitle();
                    }
                    if (cam.rotation.has_value()) {
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat3("##crot", cam.rotation->data(), 0.5f)) {
                            if (ImGui::IsItemActivated()) pushUndo();
                            modified_ = true; updateWindowTitle();
                        }
                    }
                }

                // Near / Far
                ImGui::TextDisabled("Near Plane");
                ImGui::SetNextItemWidth(-1);
                // AlwaysClamp: keeps the near/far invariant (near <= far)
                // enforced even against a Ctrl+Click typed value, not just
                // the drag gesture.
                if (ImGui::DragFloat("##cnear", &cam.nearPlane, 0.01f, 0.001f, cam.farPlane, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Far Plane");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##cfar", &cam.farPlane, 1.0f, cam.nearPlane, 100000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true; updateWindowTitle();
                }

                // Perspective-only: FOV
                if (cam.type == Mc3::CameraType::Perspective) {
                    ImGui::TextDisabled("FOV (deg)");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderFloat("##cfov", &cam.fov, 1.0f, 170.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }
                }

                // Orthographic-only: ortho size
                if (cam.type == Mc3::CameraType::Orthographic) {
                    ImGui::TextDisabled("Ortho Size");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##cortho", &cam.orthoSize, 0.1f, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true; updateWindowTitle();
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Textures
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Tex")) {
            // Validate selection
            if (!selectedTextureKey_.empty() &&
                !document_.textures.count(selectedTextureKey_))
                selectedTextureKey_.clear();

            // Toolbar: Add / Remove
            if (ImGui::SmallButton("+")) {
                pushUndo();
                int n = 1;
                std::string key;
                do { key = "tex_" + std::to_string(n++); }
                while (document_.textures.count(key));
                Mc3::Mc3Texture tex;
                tex.name = key;
                document_.textures[key] = tex;
                selectedTextureKey_ = key;
                modified_ = true; updateWindowTitle();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("-") && !selectedTextureKey_.empty()) {
                pushUndo();
                document_.textures.erase(selectedTextureKey_);
                selectedTextureKey_.clear();
                modified_ = true; updateWindowTitle();
            }

            // List
            ImGui::Separator();
            for (const auto& [key, tex] : document_.textures) {
                bool sel = (key == selectedTextureKey_);
                std::string label = key;
                if (!tex.uri.empty()) {
                    // Show just filename part
                    auto slash = tex.uri.find_last_of("/\\");
                    label += "  " + (slash != std::string::npos ? tex.uri.substr(slash+1) : tex.uri);
                }
                ImGui::PushID(key.c_str());
                if (ImGui::Selectable(label.c_str(), sel))
                    selectedTextureKey_ = key;
                ImGui::PopID();
            }

            // Inline editor
            if (!selectedTextureKey_.empty() &&
                document_.textures.count(selectedTextureKey_))
            {
                auto& tex = document_.textures[selectedTextureKey_];
                ImGui::Separator();
                ImGui::Spacing();

                // ID (read-only) with copy button
                ImGui::TextDisabled("ID");
                ImGui::SameLine();
                if (ImGui::SmallButton("Copy##texid"))
                    ImGui::SetClipboardText(selectedTextureKey_.c_str());
                ImGui::TextUnformatted(selectedTextureKey_.c_str());

                // Display name
                ImGui::TextDisabled("Name");
                {
                    char buf[128];
                    std::strncpy(buf, tex.name.c_str(), sizeof(buf)-1); buf[127]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##texname", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); tex.name = buf; modified_ = true; updateWindowTitle();
                    }
                }

                // URI (file path)
                ImGui::TextDisabled("URI (path)");
                {
                    char buf[512];
                    std::strncpy(buf, tex.uri.c_str(), sizeof(buf)-1); buf[511]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##texuri", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); tex.uri = buf; modified_ = true; updateWindowTitle();
                    }
                }

                // Wrap U
                {
                    const char* wraps[] = { "repeat", "clamp", "mirror" };
                    int widx = 0;
                    for (int i = 0; i < 3; ++i) if (tex.wrapU == wraps[i]) { widx = i; break; }
                    ImGui::TextDisabled("Wrap U");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##twrapu", &widx, wraps, 3)) {
                        pushUndo(); tex.wrapU = wraps[widx]; modified_ = true; updateWindowTitle();
                    }
                }

                // Wrap V
                {
                    const char* wraps[] = { "repeat", "clamp", "mirror" };
                    int widx = 0;
                    for (int i = 0; i < 3; ++i) if (tex.wrapV == wraps[i]) { widx = i; break; }
                    ImGui::TextDisabled("Wrap V");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##twrapv", &widx, wraps, 3)) {
                        pushUndo(); tex.wrapV = wraps[widx]; modified_ = true; updateWindowTitle();
                    }
                }

                // Filter
                {
                    const char* filters[] = { "linear", "nearest" };
                    int fidx = (tex.filter == "nearest") ? 1 : 0;
                    ImGui::TextDisabled("Filter");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##tfilter", &fidx, filters, 2)) {
                        pushUndo(); tex.filter = filters[fidx]; modified_ = true; updateWindowTitle();
                    }
                }

                // Color space
                {
                    const char* spaces[] = { "srgb", "linear" };
                    int sidx = (tex.colorSpace == "linear") ? 1 : 0;
                    ImGui::TextDisabled("Color Space");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##tcolorsp", &sidx, spaces, 2)) {
                        pushUndo(); tex.colorSpace = spaces[sidx]; modified_ = true; updateWindowTitle();
                    }
                }
            }

            // STAB-0703: SVG textures (N1, doc.svgTextures) previously had
            // zero editor UI at all -- a user could only assign/create one
            // by hand-editing XML. Mirrors the raster-texture list/editor
            // pattern above, in the same tab since both are "textures" from
            // a user's perspective.
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextUnformatted("SVG Textures");
            ImGui::Separator();

            if (!selectedSvgTextureKey_.empty() &&
                !document_.svgTextures.count(selectedSvgTextureKey_))
                selectedSvgTextureKey_.clear();

            if (ImGui::SmallButton("+##svgadd")) {
                pushUndo();
                int n = 1;
                std::string key;
                do { key = "svg_" + std::to_string(n++); }
                while (document_.svgTextures.count(key));
                Mc3::Mc3SvgTexture svg;
                svg.id = key;
                document_.svgTextures[key] = svg;
                selectedSvgTextureKey_ = key;
                modified_ = true; updateWindowTitle();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("-##svgremove") && !selectedSvgTextureKey_.empty()) {
                pushUndo();
                document_.svgTextures.erase(selectedSvgTextureKey_);
                selectedSvgTextureKey_.clear();
                modified_ = true; updateWindowTitle();
            }

            for (const auto& [key, svg] : document_.svgTextures) {
                bool sel = (key == selectedSvgTextureKey_);
                std::string label = key;
                if (!svg.src.empty()) {
                    auto slash = svg.src.find_last_of("/\\");
                    label += "  " + (slash != std::string::npos ? svg.src.substr(slash+1) : svg.src);
                } else if (!svg.inlineContent.empty()) {
                    label += "  (inline)";
                }
                ImGui::PushID(("svg_" + key).c_str());
                if (ImGui::Selectable(label.c_str(), sel))
                    selectedSvgTextureKey_ = key;
                ImGui::PopID();
            }

            if (!selectedSvgTextureKey_.empty() &&
                document_.svgTextures.count(selectedSvgTextureKey_))
            {
                auto& svg = document_.svgTextures[selectedSvgTextureKey_];
                ImGui::Spacing();

                ImGui::TextDisabled("ID");
                ImGui::SameLine();
                if (ImGui::SmallButton("Copy##svgid"))
                    ImGui::SetClipboardText(selectedSvgTextureKey_.c_str());
                ImGui::TextUnformatted(selectedSvgTextureKey_.c_str());

                // External vs inline are mutually exclusive per
                // Mc3SvgTexture's own contract (src empty <=> inline set).
                bool isInline = svg.isInline();
                ImGui::TextDisabled("Type");
                if (ImGui::RadioButton("External##svgtype", !isInline)) {
                    if (isInline) {
                        pushUndo(); svg.inlineContent.clear(); modified_ = true; updateWindowTitle();
                    }
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Inline##svgtype", isInline)) {
                    if (!isInline) {
                        pushUndo(); svg.src.clear(); modified_ = true; updateWindowTitle();
                    }
                }

                if (!isInline) {
                    ImGui::TextDisabled("Source path (.svg)");
                    char buf[512];
                    std::strncpy(buf, svg.src.c_str(), sizeof(buf)-1); buf[511]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##svgsrc", buf, sizeof(buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); svg.src = buf; modified_ = true; updateWindowTitle();
                    }
                } else {
                    ImGui::TextDisabled("Inline SVG markup");
                    char buf[8192];
                    std::strncpy(buf, svg.inlineContent.c_str(), sizeof(buf)-1); buf[8191]='\0';
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputTextMultiline("##svginline", buf, sizeof(buf),
                            ImVec2(-1, 120), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        pushUndo(); svg.inlineContent = buf; modified_ = true; updateWindowTitle();
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Definitions
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Defs")) {
            // Validate selection
            if (!selectedDefId_.empty() && !document_.definitions.count(selectedDefId_))
                selectedDefId_.clear();

            // Toolbar: Add / Remove
            if (ImGui::SmallButton("+")) {
                pushUndo();
                int n = 1;
                std::string key;
                do { key = "def_" + std::to_string(n++); }
                while (document_.definitions.count(key));
                auto defObj = std::make_shared<Mc3::Mc3Object>();
                defObj->id   = key;
                defObj->name = key;
                defObj->type = Mc3::ObjectType::Box;
                Mc3::Mc3Primitive p; p.primitiveType = Mc3::PrimitiveType::Box; p.size = {1.0f,1.0f,1.0f};
                defObj->primitive = p;
                document_.definitions[key] = defObj;
                selectedDefId_ = key;
                modified_ = true; updateWindowTitle();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(selectedDefId_.empty());
            if (ImGui::SmallButton("-")) {
                pushUndo();
                document_.definitions.erase(selectedDefId_);
                selectedDefId_.clear();
                modified_ = true; updateWindowTitle();
            }
            ImGui::EndDisabled();

            // List
            ImGui::Separator();
            for (const auto& [key, defObj] : document_.definitions) {
                bool sel = (key == selectedDefId_);
                std::string label = key;
                if (defObj && !defObj->name.empty() && defObj->name != key)
                    label += "  (" + defObj->name + ")";
                ImGui::PushID(key.c_str());
                if (ImGui::Selectable(label.c_str(), sel))
                    selectedDefId_ = key;
                ImGui::PopID();
            }

            // Inline editor for selected definition
            if (!selectedDefId_.empty() && document_.definitions.count(selectedDefId_)) {
                auto& defObj = document_.definitions[selectedDefId_];
                if (defObj) {
                    ImGui::Separator();
                    ImGui::Spacing();

                    // ID (rename — updates map key + all Instance references)
                    ImGui::TextDisabled("ID");
                    {
                        char idBuf[128];
                        std::strncpy(idBuf, selectedDefId_.c_str(), sizeof(idBuf)-1);
                        idBuf[127] = '\0';
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::InputText("##defid", idBuf, sizeof(idBuf),
                                ImGuiInputTextFlags_EnterReturnsTrue)) {
                            std::string newKey = idBuf;
                            if (!newKey.empty() && newKey != selectedDefId_ &&
                                !document_.definitions.count(newKey)) {
                                pushUndo();
                                // Move entry to new key
                                auto node = document_.definitions.extract(selectedDefId_);
                                node.key() = newKey;
                                document_.definitions.insert(std::move(node));
                                defObj->id = newKey;
                                // Update all Instance references in the scene
                                std::function<void(std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> fixRefs;
                                fixRefs = [&](auto& list) {
                                    for (auto& o : list) {
                                        if (o->type == Mc3::ObjectType::Instance) {
                                            if (o->definition == selectedDefId_)
                                                o->definition = newKey;
                                            for (auto& v : o->variantDefinitions)
                                                if (v == selectedDefId_) v = newKey;
                                        }
                                        fixRefs(o->children);
                                    }
                                };
                                fixRefs(document_.objects);
                                selectedDefId_ = newKey;
                                modified_ = true; updateWindowTitle();
                            }
                        }
                    }

                    // Name
                    ImGui::TextDisabled("Name");
                    {
                        char nameBuf[128];
                        std::strncpy(nameBuf, defObj->name.c_str(), sizeof(nameBuf)-1);
                        nameBuf[127] = '\0';
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::InputText("##defname", nameBuf, sizeof(nameBuf),
                                ImGuiInputTextFlags_EnterReturnsTrue)) {
                            pushUndo(); defObj->name = nameBuf; modified_ = true; updateWindowTitle();
                        }
                    }

                    // Type
                    ImGui::TextDisabled("Type");
                    {
                        const char* typeNames[] = { "Box","Sphere","Cylinder","Cone","Plane",
                                                    "Extrude","Group","Mesh","Area" };
                        Mc3::ObjectType typeVals[] = {
                            Mc3::ObjectType::Box, Mc3::ObjectType::Sphere,
                            Mc3::ObjectType::Cylinder, Mc3::ObjectType::Cone,
                            Mc3::ObjectType::Plane, Mc3::ObjectType::Extrude,
                            Mc3::ObjectType::Group, Mc3::ObjectType::Mesh,
                            Mc3::ObjectType::Area };
                        int tidx = 0;
                        for (int i = 0; i < 9; ++i)
                            if (defObj->type == typeVals[i]) { tidx = i; break; }
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::Combo("##deftype", &tidx, typeNames, 9)) {
                            pushUndo(); defObj->type = typeVals[tidx]; modified_ = true; updateWindowTitle();
                        }
                    }

                    // Transform
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::TextDisabled("Position");
                    {
                        float pos[3] = { defObj->transform.position[0], defObj->transform.position[1], defObj->transform.position[2] };
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat3("##dpos", pos, 0.1f)) {
                            if (ImGui::IsItemActivated()) pushUndo();
                            defObj->transform.position[0] = pos[0];
                            defObj->transform.position[1] = pos[1];
                            defObj->transform.position[2] = pos[2];
                            modified_ = true; updateWindowTitle();
                        }
                    }
                    ImGui::TextDisabled("Rotation");
                    {
                        float rot[3] = { defObj->transform.rotation[0], defObj->transform.rotation[1], defObj->transform.rotation[2] };
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat3("##drot", rot, 0.5f)) {
                            if (ImGui::IsItemActivated()) pushUndo();
                            defObj->transform.rotation[0] = rot[0];
                            defObj->transform.rotation[1] = rot[1];
                            defObj->transform.rotation[2] = rot[2];
                            modified_ = true; updateWindowTitle();
                        }
                    }
                    ImGui::TextDisabled("Scale");
                    {
                        float scl[3] = { defObj->transform.scale[0], defObj->transform.scale[1], defObj->transform.scale[2] };
                        ImGui::SetNextItemWidth(-1);
                        // AlwaysClamp: the callback already clamps scale
                        // defensively (std::max below), this keeps the
                        // widget's own displayed value consistent within
                        // the same frame.
                        if (ImGui::DragFloat3("##dscl", scl, 0.01f, 0.001f, 100.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                            if (ImGui::IsItemActivated()) pushUndo();
                            defObj->transform.scale[0] = std::max(0.001f, scl[0]);
                            defObj->transform.scale[1] = std::max(0.001f, scl[1]);
                            defObj->transform.scale[2] = std::max(0.001f, scl[2]);
                            modified_ = true; updateWindowTitle();
                        }
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------------
        // Tab: Materials
        // -------------------------------------------------------------------
        if (ImGui::BeginTabItem("Mat")) {
            // Validate selection
            if (!selectedMaterialKey_.empty() && !document_.materials.count(selectedMaterialKey_))
                selectedMaterialKey_.clear();

            // Toolbar: Add
            if (ImGui::SmallButton("+##matadd")) {
                pushUndo();
                int n = 1;
                std::string key;
                do { key = "mat_" + std::to_string(n++); }
                while (document_.materials.count(key));
                Mc3::Mc3Material m;
                m.name = key;
                document_.materials[key] = m;
                selectedMaterialKey_ = key;
                modified_ = true; updateWindowTitle();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add material");
            ImGui::SameLine();
            {
                bool canRemove = !selectedMaterialKey_.empty();
                if (!canRemove) ImGui::BeginDisabled();
                if (ImGui::SmallButton("×##matrem")) {
                    pushUndo();
                    document_.materials.erase(selectedMaterialKey_);
                    selectedMaterialKey_.clear();
                    modified_ = true; updateWindowTitle();
                }
                if (!canRemove) ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Remove selected material");
            ImGui::SameLine();
            {
                bool canApply = !selectedMaterialKey_.empty() && selection_.hasSelection();
                if (!canApply) ImGui::BeginDisabled();
                if (ImGui::SmallButton("Apply##matapply")) {
                    pushUndo();
                    for (const auto& s : selection_.selection()) {
                        if (!lockedIds_.count(s->id))
                            s->material = selectedMaterialKey_;
                    }
                    modified_ = true; updateWindowTitle();
                    setStatusMsg("Material '" + selectedMaterialKey_ + "' applied to " +
                                 std::to_string(selection_.selection().size()) + " object(s)");
                }
                if (!canApply) ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip(selection_.hasSelection()
                    ? "Apply selected material to all selected objects"
                    : "Select objects in the scene first");
            ImGui::SameLine();
            // Export (D5)
            {
                bool canExp = !selectedMaterialKey_.empty();
                if (!canExp) ImGui::BeginDisabled();
                if (ImGui::SmallButton("Exp##matexp")) {
                    matExportOpen_ = true;
                    matExportId_   = selectedMaterialKey_;
                    std::string def = selectedMaterialKey_ + ".mc3mat.xml";
                    std::strncpy(matExportBuf_, def.c_str(), sizeof(matExportBuf_)-1);
                    matExportBuf_[sizeof(matExportBuf_)-1] = '\0';
                    matExportErr_[0] = '\0';
                }
                if (!canExp) ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Export selected material to .mc3mat.xml file");
            ImGui::SameLine();
            // Import (D5)
            if (ImGui::SmallButton("Imp##matimp")) {
                matImportOpen_ = true;
                matImportBuf_[0] = '\0';
                matImportErr_[0] = '\0';
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Import material from .mc3mat.xml file");
            ImGui::Separator();

            // Search filter
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##matfilter", "Search materials…", matFilter_, sizeof(matFilter_));
            if (ImGui::IsItemHovered() && matFilter_[0] != '\0') {
                ImGui::SameLine();
            }
            if (matFilter_[0] != '\0') {
                ImGui::SameLine();
                if (ImGui::SmallButton("×##matfilterclear")) matFilter_[0] = '\0';
            }

            // Build lowercase filter string
            std::string matFiltLow(matFilter_);
            for (auto& ch : matFiltLow)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

            // Material list
            int matShown = 0;
            for (auto& [key, mat] : document_.materials) {
                // Apply filter
                if (!matFiltLow.empty()) {
                    std::string keyLow = key;
                    for (auto& ch : keyLow)
                        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    if (keyLow.find(matFiltLow) == std::string::npos) continue;
                }
                ++matShown;
                bool isSel = (key == selectedMaterialKey_);
                // Colored square preview
                ImGui::PushID(key.c_str());
                ImVec4 c(mat.baseColor[0], mat.baseColor[1], mat.baseColor[2], 1.0f);
                ImGui::ColorButton("##cb", c,
                    ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                    ImVec2(14, 14));
                ImGui::SameLine(0, 4);
                if (ImGui::Selectable(key.c_str(), isSel,
                                      ImGuiSelectableFlags_SpanAllColumns,
                                      ImVec2(0, 0)))
                    selectedMaterialKey_ = key;
                ImGui::PopID();
            }
            if (document_.materials.empty())
                ImGui::TextDisabled("No materials — press + to add");
            else if (matShown == 0)
                ImGui::TextDisabled("No match for \"%s\"", matFilter_);

            // Editor for selected material
            // AUDIT-0055: intentionally scalars/colors only (baseColor,
            // roughness, metallic, emissive, alphaMode, doubleSided) — this
            // is the quick-access left panel. Texture-slot assignment
            // (baseColorTexture, normalTexture, metallicRoughnessTexture,
            // emissiveTexture, occlusionTexture) is only in the full
            // PropertiesPanel.cpp material editor, by design.
            if (!selectedMaterialKey_.empty() && document_.materials.count(selectedMaterialKey_)) {
                auto& mat = document_.materials[selectedMaterialKey_];
                ImGui::Separator();

                // D7: Material preview sphere
                initMatPreview();
                renderMatPreview(mat.baseColor[0], mat.baseColor[1], mat.baseColor[2],
                                 mat.roughness, mat.metallic);
                if (matPreviewTexId_) {
                    float avail = ImGui::GetContentRegionAvail().x;
                    float sz = std::min(avail, (float)kMatPreviewRes);
                    float off = (avail - sz) * 0.5f;
                    if (off > 0.f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
                    ImGui::Image((ImTextureID)(intptr_t)matPreviewTexId_, ImVec2(sz, sz));
                    ImGui::Spacing();
                }

                // Rename key
                {
                    char kbuf[128];
                    std::strncpy(kbuf, selectedMaterialKey_.c_str(), sizeof(kbuf) - 1);
                    kbuf[sizeof(kbuf)-1] = '\0';
                    ImGui::TextDisabled("ID");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##matkey", kbuf, sizeof(kbuf),
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                        std::string newKey(kbuf);
                        if (!newKey.empty() && !document_.materials.count(newKey)) {
                            pushUndo();
                            Mc3::Mc3Material tmp = mat;
                            document_.materials.erase(selectedMaterialKey_);
                            // Update any objects referencing the old key
                            std::function<void(std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> fixRefs;
                            fixRefs = [&](auto& list) {
                                for (auto& o : list) {
                                    if (o->material == selectedMaterialKey_) o->material = newKey;
                                    fixRefs(o->children);
                                }
                            };
                            fixRefs(document_.objects);
                            document_.materials[newKey] = std::move(tmp);
                            selectedMaterialKey_ = newKey;
                            modified_ = true; updateWindowTitle();
                        }
                    }
                }

                // AUDIT-0059: material-editor widgets below were missing
                // pushUndo() entirely (unlike PropertiesPanel.cpp's own copy
                // of this material editor, which already has it) --
                // IsItemActivated()-gated for the continuous-drag widgets.

                // Base color
                ImGui::TextDisabled("Base Color");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit4("##matbc", mat.baseColor.data(),
                    ImGuiColorEditFlags_Float)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true;
                }

                // Roughness
                ImGui::TextDisabled("Roughness");
                ImGui::SetNextItemWidth(-1);
                // AlwaysClamp: without it, Ctrl+Click lets a typed value go
                // out of [0,1], which would export a spec-invalid glTF
                // pbrMetallicRoughness.roughnessFactor with no other guard.
                if (ImGui::SliderFloat("##matrgh", &mat.roughness, 0.0f, 1.0f, "%.2f",
                                       ImGuiSliderFlags_AlwaysClamp)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true;
                }

                // Metallic
                ImGui::TextDisabled("Metallic");
                ImGui::SetNextItemWidth(-1);
                // AlwaysClamp: same out-of-[0,1]-via-Ctrl+Click risk as
                // roughness above (spec-invalid metallicFactor on export).
                if (ImGui::SliderFloat("##matmet", &mat.metallic, 0.0f, 1.0f, "%.2f",
                                       ImGuiSliderFlags_AlwaysClamp)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true;
                }

                // Emissive
                ImGui::TextDisabled("Emissive");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::ColorEdit3("##matemi", mat.emissiveColor.data(),
                    ImGuiColorEditFlags_Float)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    modified_ = true;
                }

                // Alpha mode
                ImGui::TextDisabled("Alpha Mode");
                const char* alphaModes[] = { "opaque", "mask", "blend" };
                int alphaIdx = 0;
                if (mat.alphaMode == "mask")  alphaIdx = 1;
                if (mat.alphaMode == "blend") alphaIdx = 2;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("##matam", &alphaIdx, alphaModes, 3)) {
                    pushUndo();
                    mat.alphaMode = alphaModes[alphaIdx];
                    modified_ = true;
                }
                if (alphaIdx == 1) {
                    ImGui::TextDisabled("Alpha Cutoff");
                    ImGui::SetNextItemWidth(-1);
                    // AlwaysClamp: same out-of-[0,1]-via-Ctrl+Click risk as
                    // roughness/metallic above (spec-invalid alphaCutoff).
                    if (ImGui::SliderFloat("##matac", &mat.alphaCutoff, 0.0f, 1.0f, "%.2f",
                                           ImGuiSliderFlags_AlwaysClamp)) {
                        if (ImGui::IsItemActivated()) pushUndo();
                        modified_ = true;
                    }
                }

                // Double-sided
                if (ImGui::Checkbox("Double-sided##matds", &mat.doubleSided)) {
                    pushUndo();
                    modified_ = true;
                }
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
    ImGui::PopStyleColor();

    // -----------------------------------------------------------------------

}


} // namespace MeshCraft
