// 2026-07-20 audit F2 (P0): SceneHierarchyPanel.cpp's drawHierarchy() used
// to range-for-iterate over document_.objects (or a nested children
// vector) by reference, while several menu/drag actions reachable from the
// CURRENTLY-RENDERING node -- drag-drop reparent, Delete, Duplicate, Group
// Selection, Set as Root -- synchronously erased/inserted into that same
// live vector mid-loop. Erasing an EARLIER sibling while the loop is
// rendering a LATER one invalidates the loop's own begin()/end() iterators
// (real UB: std::vector::erase() invalidates iterators at and after the
// erased position, which includes the loop's cached end()). This is not a
// contrived edge case -- dragging one root-level object onto another is an
// entirely ordinary, everyday interaction.
//
// Drives a REAL multi-frame ImGui drag gesture between two actual tree
// rows rendered by the real SceneHierarchyPanel class (not a mock),
// mirroring undo_gesture_frame_test.cpp's driveClickDragGesture technique.
// Row positions are computed deterministically rather than hardcoded:
// Cube and Sphere are both root-level leaf objects (no expand arrow, no
// children), so they occupy exactly one text line each, back to back --
// GetItemRectMin()/Max() captured right after SceneHierarchyPanel::draw()
// returns gives the LAST item's rect (Sphere's row, since its lock/
// visibility SmallButtons are the final items SameLine()'d onto that same
// row), and Cube's row sits exactly one GetTextLineHeightWithSpacing()
// above it.

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include "MeshCraft/Scene/SceneHierarchyPanel.hpp"
#include "MeshCraft/Editor/SelectionManager.hpp"
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <iostream>
#include <set>
#include <string>

using namespace MeshCraft;
using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pix; int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pix, &w, &h);
    io.Fonts->SetTexID(static_cast<ImTextureID>(1));

    Mc3Document doc;
    doc.model = "DragDropTest";
    // Mc3Object::makeBox()/makeSphere() only set `name`, not `id` --
    // SceneHierarchyPanel's drag payload/findObj/detachObj all key off
    // `id`, not `name`, so both must be set explicitly and distinctly.
    auto cube   = Mc3Object::makeBox("cube");
    cube->id    = "cube";
    auto sphere = Mc3Object::makeSphere("sphere");
    sphere->id  = "sphere";
    doc.objects.push_back(cube);
    doc.objects.push_back(sphere);

    Scene::SceneHierarchyPanel panel(doc);
    Editor::SelectionManager selection;
    std::set<std::string> lockedIds;
    Scene::HierarchyCallbacks cb;
    cb.pushUndo       = []{};
    cb.markModified   = []{};
    cb.duplicateSel   = []{};
    cb.deleteSel      = []{};
    cb.selectParent   = []{};
    cb.selectChildren = []{};
    cb.openBatchRename = []{};

    auto frame = [&](float mx, float my, bool mouseDown) {
        io.AddMousePosEvent(mx, my);
        io.AddMouseButtonEvent(0, mouseDown);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(400, 500));
        ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoSavedSettings);
        panel.draw(selection, lockedIds, cb);
        ImVec2 mn = ImGui::GetItemRectMin(), mx2 = ImGui::GetItemRectMax();
        ImGui::End();
        ImGui::Render();
        return std::pair{mn, mx2};
    };

    // Frame 0: layout-only pass, mouse away from everything. draw()'s very
    // last item is the root-level drop zone's `ImGui::Dummy(-1, 10)`, which
    // immediately follows Sphere's row (the last tree row) in the vertical
    // layout -- so Sphere's row bottom sits exactly one ItemSpacing.y above
    // the Dummy's top, and Cube's row (the row directly above Sphere's) is
    // exactly one GetTextLineHeightWithSpacing() above Sphere's.
    auto [dummyMin, dummyMax] = frame(700, 700, false);
    (void)dummyMax;
    const ImGuiStyle& style = ImGui::GetStyle();
    float lineH        = ImGui::GetTextLineHeight();
    float lineHSpacing = ImGui::GetTextLineHeightWithSpacing();

    // A fixed X well left of the right-aligned lock/visibility buttons
    // (those sit at GetWindowContentRegionMax().x - 36/-18, i.e. near the
    // window's right edge) and well right of the tree-arrow toggle itself,
    // landing on each row's label area.
    float clickX  = 60.0f;
    float sphereRowBottom = dummyMin.y - style.ItemSpacing.y;
    float sphereY = sphereRowBottom - lineH * 0.5f;
    float cubeY   = sphereY - lineHSpacing;

    check(cubeY > 0.0f && cubeY < sphereY,
          "Cube's computed row Y is above Sphere's and still on-screen (sanity check on the layout math)");

    // Drive the drag gesture: hover Cube, press, drag toward Sphere over
    // several frames (past ImGui's drag threshold, mirroring
    // driveClickDragGesture's own 20-move-frame pattern), release over
    // Sphere -- accepting the drop there.
    frame(clickX, cubeY, false);                     // hover Cube
    frame(clickX, cubeY, true);                       // press on Cube (activation frame)
    for (int i = 1; i <= 10; ++i) {
        float t = static_cast<float>(i) / 10.0f;
        float y = cubeY + (sphereY - cubeY) * t;
        frame(clickX, y, true);                        // drag toward Sphere, still held
    }
    frame(clickX, sphereY, true);                      // hovering directly over Sphere, still held
    frame(clickX, sphereY, false);                      // release over Sphere -- drop accepted here

    // One more idle frame: not strictly required (the fix flushes
    // pendingAction synchronously within the SAME draw() call the drop was
    // accepted in), but confirms the panel keeps rendering cleanly
    // afterward with no leftover corrupted state.
    frame(700, 700, false);

    check(doc.objects.size() == 1,
          "after the drop, exactly one root-level object remains (got " +
          std::to_string(doc.objects.size()) + ")");
    if (doc.objects.size() == 1) {
        check(doc.objects[0]->id == "sphere",
              "the remaining root object is Sphere (Cube was reparented under it), got id='" +
              doc.objects[0]->id + "'");
        check(doc.objects[0]->children.size() == 1,
              "Sphere has exactly one child after the drop (got " +
              std::to_string(doc.objects[0]->children.size()) + ")");
        if (doc.objects[0]->children.size() == 1) {
            check(doc.objects[0]->children[0]->id == "cube",
                  "Sphere's child is Cube, got id='" + doc.objects[0]->children[0]->id + "'");
        }
    }

    ImGui::DestroyContext();

    if (failures == 0) { std::cout << "All scene_hierarchy_panel tests passed.\n"; return 0; }
    std::cerr << failures << " scene_hierarchy_panel test(s) FAILED.\n";
    return 1;
}
