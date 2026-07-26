#include "MeshCraft/Application/MeshCraftApplication.hpp"
#include "MeshCraft/CoordinateSystemAlgorithms.hpp"
#include "MeshCraft/RotationConventionCna.hpp"
#include "MeshCraft/RotationConventionAlgorithms.hpp"
#include "MeshCraft/MeshCraftPrivate.hpp"
#include "MeshCraft/EditorSelectionAlgorithms.hpp"
#include "MeshCraft/EditorTransformAlgorithms.hpp"

#include <Microsoft/Xna/Framework/Input/Keys.hpp>
#include <Microsoft/Xna/Framework/Input/KeyboardState.hpp>
#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <Microsoft/Xna/Framework/Color.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <numbers>
#include <string>

namespace MeshCraft::Application {

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;
using namespace Microsoft::Xna::Framework::Graphics;

void MeshCraftApplication::handleMouseInput(const MouseState& ms, const MouseState& prev) {
    int dx = ms.getXProperty() - prev.getXProperty();
    int dy = ms.getYProperty() - prev.getYProperty();
    int dscroll = ms.getScrollWheelValueProperty() - prev.getScrollWheelValueProperty();

    bool leftBtn   = ms.getLeftButtonProperty()   == ButtonState::Pressed;
    bool rightBtn  = ms.getRightButtonProperty()  == ButtonState::Pressed;
    bool middleBtn = ms.getMiddleButtonProperty() == ButtonState::Pressed;
    bool prevLeft  = prev.getLeftButtonProperty() == ButtonState::Pressed;

    // End gizmo drag on mouse release
    if (!leftBtn && gizmo_.isDragging())
        gizmo_.endDrag();

    // Camera orbit / pan / zoom
    if (middleBtn && (dx != 0 || dy != 0))
        camera_.orbit(dx * 0.005f, dy * 0.005f);
    else if (rightBtn && !middleBtn && (dx != 0 || dy != 0))
        camera_.pan(static_cast<float>(-dx), static_cast<float>(dy));

    if (dscroll != 0)
        camera_.zoom(static_cast<float>(dscroll) / 120.0f);

    // Compute 3D viewport bounds (same formula as Draw())
    // Use cachedScreenW_/H_ set each frame by Draw() from io.DisplaySize (logical pixels)
    // — avoids the one-frame lag that gd.getViewportProperty() has after window resize.
    auto& gd = getGraphicsDeviceProperty();
    int screenW = cachedScreenW_ > 1 ? cachedScreenW_ : gd.getViewportProperty().getWidthProperty();
    int screenH = cachedScreenH_ > 1 ? cachedScreenH_ : gd.getViewportProperty().getHeightProperty();
    int topH    = imguiTopH_ > 0 ? imguiTopH_ : 60;
    int vX = kLeftPanelW, vY = topH;
    int vW = std::max(1, screenW - kLeftPanelW - kRightPanelW);
    int tlH = showTimeline_ ? kTimelineH : 0;
    int vH = std::max(1, screenH - topH - tlH - kStatusH);
    float asp = static_cast<float>(vW) / static_cast<float>(vH);

    // Helper: compute axis-aligned bounding box for an object
    auto objectAABB = [](const Mc3::Mc3Object& obj, Vector3& bMin, Vector3& bMax) {
        const auto& t = obj.transform;
        float opx = t.position[0], opy = t.position[1], opz = t.position[2];
        float osx = t.scale[0],    osy = t.scale[1],    osz = t.scale[2];
        float hx = 0.5f, hy = 0.5f, hz = 0.5f;
        if (obj.primitive) {
            const auto& p = *obj.primitive;
            switch (p.primitiveType) {
            case Mc3::PrimitiveType::Box: case Mc3::PrimitiveType::Cube:
                hx = p.size[0]*0.5f; hy = p.size[1]*0.5f; hz = p.size[2]*0.5f; break;
            case Mc3::PrimitiveType::Sphere:
                hx = hy = hz = p.radius; break;
            case Mc3::PrimitiveType::Cylinder: case Mc3::PrimitiveType::Cone:
                hx = hz = p.radius; hy = p.height*0.5f; break;
            case Mc3::PrimitiveType::Plane:
                hx = p.size[0]*0.5f; hy = 0.05f; hz = p.size[1]*0.5f; break;
            default: break;
            }
        }
        hx *= std::abs(osx); hy *= std::abs(osy); hz *= std::abs(osz);
        bMin = {opx-hx, opy-hy, opz-hz};
        bMax = {opx+hx, opy+hy, opz+hz};
    };

    // Helper: compute local or world axis vectors for a given object
    auto getLocalAxes = [&](const Mc3::Mc3Object* obj, Vector3 axes[3]) {
        if (gizmoLocalSpace_) {
            Matrix rotM = MeshCraft::rotationMatrixForDocumentAlg(document_, obj->transform.rotation);
            axes[0] = rotM.getRightProperty();
            axes[1] = rotM.getUpProperty();
            Vector3 fwd = rotM.getForwardProperty();
            axes[2] = {-fwd.X, -fwd.Y, -fwd.Z};
        } else {
            axes[0] = {1,0,0};
            axes[1] = {0,1,0};
            axes[2] = {0,0,1};
        }
    };

    // Apply gizmo drag (Move)
    if (activeTool_ == ActiveTool::Move && gizmo_.isDragging() && leftBtn && (dx != 0 || dy != 0)
        && selection_.hasSelection())
    {
        Matrix vw = camera_.viewMatrix();
        Matrix pr = camera_.projectionMatrix(asp);
        Matrix vp = vw * pr;

        auto w2s = [&](float wx, float wy, float wz) -> std::pair<float,float> {
            const auto yUp = coordinateToYUpAlg(document_.coordinateSystem, {wx, wy, wz});
            wx = yUp[0]; wy = yUp[1]; wz = yUp[2];
            float cX = wx*vp.M11 + wy*vp.M21 + wz*vp.M31 + vp.M41;
            float cY = wx*vp.M12 + wy*vp.M22 + wz*vp.M32 + vp.M42;
            float cW = wx*vp.M14 + wy*vp.M24 + wz*vp.M34 + vp.M44;
            if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
            return { (cX/cW * 0.5f + 0.5f) * vW + vX,
                     (1.0f - (cY/cW * 0.5f + 0.5f)) * vH + vY };
        };

        auto* sel0 = selection_.selection().front().get();
        // In pivot edit mode the gizmo lives at position + pivot
        float px = sel0->transform.position[0] + (pivotEditMode_ ? sel0->transform.pivot[0] : 0.0f);
        float py2= sel0->transform.position[1] + (pivotEditMode_ ? sel0->transform.pivot[1] : 0.0f);
        float pz = sel0->transform.position[2] + (pivotEditMode_ ? sel0->transform.pivot[2] : 0.0f);
        float L  = camera_.distance * 0.15f;
        int axIdx = static_cast<int>(gizmo_.dragAxis()) - 1;

        Vector3 localAxes[3];
        getLocalAxes(sel0, localAxes);
        const Vector3& ax = localAxes[axIdx];
        float tipX = px + L*ax.X, tipY = py2 + L*ax.Y, tipZ = pz + L*ax.Z;

        auto [cx, cy] = w2s(px, py2, pz);
        auto [tx, ty] = w2s(tipX, tipY, tipZ);
        float axScrX = tx - cx, axScrY = ty - cy;
        float len2d  = std::sqrt(axScrX*axScrX + axScrY*axScrY);
        if (len2d > 0.5f) {
            float delta = dx * (axScrX/len2d) + dy * (axScrY/len2d);
            delta *= L / len2d;
            for (const auto& s : selection_.selection()) {
                if (objectLockState_.isLocked(s->id)) continue;
                if (pivotEditMode_) {
                    // Move pivot only; compensate position so geometry stays in world space.
                    // pos_new = pos_old + d*R - d  (where d = delta*axis, R = object rotation matrix)
                    const auto& rot = s->transform.rotation;
                    Matrix R = MeshCraft::rotationMatrixForDocumentAlg(document_, rot);
                    float dX = delta * ax.X, dY = delta * ax.Y, dZ = delta * ax.Z;
                    // d * R (row vector × matrix)
                    float rX = dX*R.M11 + dY*R.M21 + dZ*R.M31;
                    float rY = dX*R.M12 + dY*R.M22 + dZ*R.M32;
                    float rZ = dX*R.M13 + dY*R.M23 + dZ*R.M33;
                    s->transform.pivot[0] += dX;
                    s->transform.pivot[1] += dY;
                    s->transform.pivot[2] += dZ;
                    s->transform.position[0] += rX - dX;
                    s->transform.position[1] += rY - dY;
                    s->transform.position[2] += rZ - dZ;
                } else {
                    s->transform.position[0] += delta * ax.X;
                    s->transform.position[1] += delta * ax.Y;
                    s->transform.position[2] += delta * ax.Z;
                    if (snapEnabled_) {
                        for (int i = 0; i < 3; ++i)
                            s->transform.position[i] = std::round(s->transform.position[i] / snapTranslate_) * snapTranslate_;
                    }
                }
            }

            // Vertex snap (Shift): snap to nearest other object's pivot
            bool shiftHeld = (Keyboard::GetState().IsKeyDown(Keys::LeftShift) ||
                              Keyboard::GetState().IsKeyDown(Keys::RightShift));
            if (shiftHeld) {
                float threshold = camera_.distance * 0.08f;
            vertexSnapToNearestAlg(document_.objects, selection_.selection(), objectLockState_.ids(),
                                       sel0->transform.position[0],
                                       sel0->transform.position[1],
                                       sel0->transform.position[2],
                                       threshold);
            }

            // Proportional editing (H1): apply Gaussian falloff to nearby unselected objects
            if (propEditEnabled_ && propEditRadius_ > 0.0f) {
            applyProportionalFalloffAlg(document_.objects, selection_.selection(), objectLockState_.ids(),
                                            delta * ax.X, delta * ax.Y, delta * ax.Z,
                                            propEditRadius_);
            }

            // Surface snap (B8): snap Y to the top surface directly below each selected object
            if (surfaceSnapEnabled_) {
                for (const auto& s : selection_.selection()) {
                    if (objectLockState_.isLocked(s->id)) continue;
                    float spx = s->transform.position[0];
                    float spz = s->transform.position[2];
                    float highY = s->transform.position[1] + 200.0f;
                    float bestSurfY = -1e30f;
                    bool  foundSurf = false;

                    std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> scanSurf;
                    scanSurf = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
                        for (const auto& obj : list) {
                            if (!obj || !obj->visible || selection_.isSelected(obj.get())) continue;
                            Vector3 bMin, bMax;
                            objectAABB(*obj, bMin, bMax);
                            // vertical ray through (spx, spz) hits the top of this AABB?
                            if (spx >= bMin.X && spx <= bMax.X &&
                                spz >= bMin.Z && spz <= bMax.Z &&
                                bMax.Y < highY && bMax.Y > bestSurfY)
                            {
                                bestSurfY = bMax.Y;
                                foundSurf = true;
                            }
                            if (!obj->children.empty()) scanSurf(obj->children);
                        }
                    };
                    scanSurf(document_.objects);

                    if (foundSurf) s->transform.position[1] = bestSurfY;
                }
            }

            modified_ = true;
            updateWindowTitle();
        }
        return;
    }

    // Apply gizmo drag (Scale)
    if (activeTool_ == ActiveTool::Scale && gizmo_.isDragging() && leftBtn && (dx != 0 || dy != 0)
        && selection_.hasSelection())
    {
        Matrix vw = camera_.viewMatrix();
        Matrix pr = camera_.projectionMatrix(asp);
        Matrix vp = vw * pr;

        auto w2s = [&](float wx, float wy, float wz) -> std::pair<float,float> {
            const auto yUp = coordinateToYUpAlg(document_.coordinateSystem, {wx, wy, wz});
            wx = yUp[0]; wy = yUp[1]; wz = yUp[2];
            float cX = wx*vp.M11 + wy*vp.M21 + wz*vp.M31 + vp.M41;
            float cY = wx*vp.M12 + wy*vp.M22 + wz*vp.M32 + vp.M42;
            float cW = wx*vp.M14 + wy*vp.M24 + wz*vp.M34 + vp.M44;
            if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
            return { (cX/cW * 0.5f + 0.5f) * vW + vX,
                     (1.0f - (cY/cW * 0.5f + 0.5f)) * vH + vY };
        };

        auto* sel0 = selection_.selection().front().get();
        float px = sel0->transform.position[0], py2 = sel0->transform.position[1], pz = sel0->transform.position[2];
        float L  = camera_.distance * 0.15f;
        int axIdx = static_cast<int>(gizmo_.dragAxis()) - 1;

        Vector3 localAxes[3];
        getLocalAxes(sel0, localAxes);
        const Vector3& ax = localAxes[axIdx];
        float tipX = px + L*ax.X, tipY = py2 + L*ax.Y, tipZ = pz + L*ax.Z;

        auto [cx, cy] = w2s(px, py2, pz);
        auto [tx, ty] = w2s(tipX, tipY, tipZ);
        float axScrX = tx - cx, axScrY = ty - cy;
        float len3d  = std::sqrt(axScrX*axScrX + axScrY*axScrY);
        if (len3d > 0.5f) {
            float delta = (dx * (axScrX/len3d) + dy * (axScrY/len3d)) / len3d;
            for (const auto& s : selection_.selection()) {
                if (objectLockState_.isLocked(s->id)) continue;
                float& sc = s->transform.scale[axIdx];
                sc = std::max(0.01f, sc + delta);
                if (snapEnabled_)
                    sc = std::max(snapScale_, std::round(sc / snapScale_) * snapScale_);
            }
            modified_ = true;
            updateWindowTitle();
        }
        return;
    }

    // Apply gizmo drag (Rotate)
    if (activeTool_ == ActiveTool::Rotate && gizmo_.isDragging() && leftBtn && (dx != 0 || dy != 0)
        && selection_.hasSelection())
    {
        Matrix vw = camera_.viewMatrix();
        Matrix pr = camera_.projectionMatrix(asp);
        Matrix vp = vw * pr;

        auto w2s = [&](float wx, float wy, float wz) -> std::pair<float,float> {
            const auto yUp = coordinateToYUpAlg(document_.coordinateSystem, {wx, wy, wz});
            wx = yUp[0]; wy = yUp[1]; wz = yUp[2];
            float cX = wx*vp.M11 + wy*vp.M21 + wz*vp.M31 + vp.M41;
            float cY = wx*vp.M12 + wy*vp.M22 + wz*vp.M32 + vp.M42;
            float cW = wx*vp.M14 + wy*vp.M24 + wz*vp.M34 + vp.M44;
            if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
            return { (cX/cW * 0.5f + 0.5f) * vW + vX,
                     (1.0f - (cY/cW * 0.5f + 0.5f)) * vH + vY };
        };

        auto* sel0 = selection_.selection().front().get();
        float px = sel0->transform.position[0], py2 = sel0->transform.position[1], pz = sel0->transform.position[2];
        float L  = camera_.distance * 0.15f;
        int axIdx = static_cast<int>(gizmo_.dragAxis()) - 1;

        auto [cx, cy] = w2s(px, py2, pz);
        float refPts[3][3] = { {px, py2+L, pz}, {px+L, py2, pz}, {px+L, py2, pz} };
        auto [rx4s, ry4s]  = w2s(refPts[axIdx][0], refPts[axIdx][1], refPts[axIdx][2]);
        float r_screen = std::max(1.0f, std::sqrt((rx4s-cx)*(rx4s-cx) + (ry4s-cy)*(ry4s-cy)));

        float curMx = static_cast<float>(ms.getXProperty());
        float curMy = static_cast<float>(ms.getYProperty());
        float radX = curMx - cx, radY = curMy - cy;
        float radLen = std::sqrt(radX*radX + radY*radY);
        if (radLen > 2.0f) {
            float tx = -radY/radLen, ty = radX/radLen;
            float degsPerPixel = 180.0f / (std::numbers::pi_v<float> * r_screen);
            float delta = MeshCraft::degreesInRotationUnitsAlg(
                (dx * tx + dy * ty) * degsPerPixel, document_.rotationUnits);
            bool ctrlHeld = (Keyboard::GetState().IsKeyDown(Keys::LeftControl) ||
                             Keyboard::GetState().IsKeyDown(Keys::RightControl));
            applyRotationDragAlg(selection_.selection(), objectLockState_.ids(), axIdx, delta,
                                 snapEnabled_ || ctrlHeld,
                                 MeshCraft::degreesInRotationUnitsAlg(snapRotate_, document_.rotationUnits));
            modified_ = true;
            updateWindowTitle();
        }
        return;
    }

    // Left click in 3D viewport
    if (leftBtn && !prevLeft) {
        int mx = ms.getXProperty();
        int my = ms.getYProperty();
        bool ctrl = (Keyboard::GetState().IsKeyDown(Keys::LeftControl) ||
                     Keyboard::GetState().IsKeyDown(Keys::RightControl));

        bool in3d = (mx >= vX && mx < vX + vW && my >= vY && my < vY + vH);
        if (in3d) {
            // Gizmo handle hit test (Move or Scale)
            if ((activeTool_ == ActiveTool::Move || activeTool_ == ActiveTool::Scale)
                && selection_.hasSelection() && !ctrl)
            {
                auto* sel0 = selection_.selection().front().get();
                float gpx = sel0->transform.position[0];
                float gpy = sel0->transform.position[1];
                float gpz = sel0->transform.position[2];
                float gL  = camera_.distance * 0.15f;

                Matrix gvw = camera_.viewMatrix();
                Matrix gpr = camera_.projectionMatrix(asp);
                Matrix gvp = gvw * gpr;

                auto gw2s = [&](float wx, float wy, float wz) -> std::pair<float,float> {
                    const auto yUp = coordinateToYUpAlg(document_.coordinateSystem, {wx, wy, wz});
                    wx = yUp[0]; wy = yUp[1]; wz = yUp[2];
                    float cX = wx*gvp.M11 + wy*gvp.M21 + wz*gvp.M31 + gvp.M41;
                    float cY = wx*gvp.M12 + wy*gvp.M22 + wz*gvp.M32 + gvp.M42;
                    float cW = wx*gvp.M14 + wy*gvp.M24 + wz*gvp.M34 + gvp.M44;
                    if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
                    return { (cX/cW * 0.5f + 0.5f) * vW + vX,
                             (1.0f - (cY/cW * 0.5f + 0.5f)) * vH + vY };
                };

                Vector3 htAxes[3];
                getLocalAxes(sel0, htAxes);
                float gTips[3][3] = {
                    {gpx+gL*htAxes[0].X, gpy+gL*htAxes[0].Y, gpz+gL*htAxes[0].Z},
                    {gpx+gL*htAxes[1].X, gpy+gL*htAxes[1].Y, gpz+gL*htAxes[1].Z},
                    {gpx+gL*htAxes[2].X, gpy+gL*htAxes[2].Y, gpz+gL*htAxes[2].Z},
                };
                for (int gi = 0; gi < 3; ++gi) {
                    auto [gsx, gsy] = gw2s(gTips[gi][0], gTips[gi][1], gTips[gi][2]);
                    float gdist = std::sqrt((mx-gsx)*(mx-gsx) + (my-gsy)*(my-gsy));
                    if (gdist < 12.0f) {
                        pushUndo();
                        gizmo_.startDrag(static_cast<Editor::GizmoAxis>(gi + 1));
                        gizmoDragAxisIdx_ = gi;
                        if (activeTool_ == ActiveTool::Move)
                            gizmoDragStartVal_ = sel0->transform.position[gi];
                        else
                            gizmoDragStartVal_ = sel0->transform.scale[gi];
                        return;
                    }
                }
            }

            // Gizmo circle hit test (Rotate)
            if (activeTool_ == ActiveTool::Rotate && selection_.hasSelection() && !ctrl) {
                auto* sel0 = selection_.selection().front().get();
                float gpx = sel0->transform.position[0];
                float gpy = sel0->transform.position[1];
                float gpz = sel0->transform.position[2];
                float gL  = camera_.distance * 0.15f;

                Matrix gvw = camera_.viewMatrix();
                Matrix gpr = camera_.projectionMatrix(asp);
                Matrix gvp = gvw * gpr;

                auto gw2s = [&](float wx, float wy, float wz) -> std::pair<float,float> {
                    const auto yUp = coordinateToYUpAlg(document_.coordinateSystem, {wx, wy, wz});
                    wx = yUp[0]; wy = yUp[1]; wz = yUp[2];
                    float cX = wx*gvp.M11 + wy*gvp.M21 + wz*gvp.M31 + gvp.M41;
                    float cY = wx*gvp.M12 + wy*gvp.M22 + wz*gvp.M32 + gvp.M42;
                    float cW = wx*gvp.M14 + wy*gvp.M24 + wz*gvp.M34 + gvp.M44;
                    if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
                    return { (cX/cW * 0.5f + 0.5f) * vW + vX,
                             (1.0f - (cY/cW * 0.5f + 0.5f)) * vH + vY };
                };

                Vector3 rotAxes[3];
                getLocalAxes(sel0, rotAxes);
                // Circle for axis[ax] lies in plane spanned by the other two axes
                Vector3 rotPlaneU[3] = { rotAxes[1], rotAxes[2], rotAxes[0] };
                Vector3 rotPlaneV[3] = { rotAxes[2], rotAxes[0], rotAxes[1] };

                const int CN = 32;
                int bestAx = -1;
                float bestDist = 11.0f;
                for (int ax = 0; ax < 3; ++ax) {
                    for (int j = 0; j < CN; ++j) {
                        float t = 2.0f * std::numbers::pi_v<float> * j / CN;
                        float c = std::cos(t), s = std::sin(t);
                        float wx = gpx + gL*(c*rotPlaneU[ax].X + s*rotPlaneV[ax].X);
                        float wy = gpy + gL*(c*rotPlaneU[ax].Y + s*rotPlaneV[ax].Y);
                        float wz = gpz + gL*(c*rotPlaneU[ax].Z + s*rotPlaneV[ax].Z);
                        auto [sx, sy] = gw2s(wx, wy, wz);
                        float d = std::sqrt((mx-sx)*(mx-sx) + (my-sy)*(my-sy));
                        if (d < bestDist) { bestDist = d; bestAx = ax; }
                    }
                }
                if (bestAx >= 0) {
                    pushUndo();
                    gizmo_.startDrag(static_cast<Editor::GizmoAxis>(bestAx + 1));
                    gizmoDragAxisIdx_  = bestAx;
                    gizmoDragStartVal_ = sel0->transform.rotation[bestAx];
                    return;
                }
            }

            // Ray-cast picking (any visible object, not just primitives — STAB-0503)
            float ndcX = ((mx - vX) / static_cast<float>(vW)) * 2.0f - 1.0f;
            float ndcY = 1.0f - ((my - vY) / static_cast<float>(vH)) * 2.0f;

            Vector3 rayOrig = camera_.position();
            Vector3 rayDir  = camera_.screenRayDirection(ndcX, ndcY, asp);

            // The camera/ray is in the editor's native Y-up space, whereas
            // pickObjectByRayAlg intentionally evaluates the document's
            // authored AABBs. Rotate the ray back into document space so
            // Z-up scenes select the same object the viewport displays.
            const auto authoredRayOrig = coordinateFromYUpAlg(
                document_.coordinateSystem, {rayOrig.X, rayOrig.Y, rayOrig.Z});
            const auto authoredRayDir = coordinateFromYUpAlg(
                document_.coordinateSystem, {rayDir.X, rayDir.Y, rayDir.Z});

            auto bestObj = pickObjectByRayAlg(
                document_.objects,
                authoredRayOrig, authoredRayDir,
                document_.rotationUnits, document_.eulerOrder);

            resolveClickSelectionAlg(selection_, bestObj, ctrl);
            // SYS-W14-40: an object click can dispatch only while the user
            // deliberately enabled Preview/Play mode.  Selection still
            // follows the ordinary editor rule; the event's own mutations
            // are isolated by EventPreviewRunner before they are committed.
            if (automationWorkspace_.previewEnabled && bestObj && !bestObj->id.empty())
                executeEventPreview(automationWorkspace_.previewRunner.dispatchClick(document_, bestObj->id));
            updateWindowTitle();
        }
    }

    // Box-select: drag in 3D viewport with Select tool
    if (activeTool_ == ActiveTool::Select && !gizmo_.isDragging()) {
        int mxB = ms.getXProperty(), myB = ms.getYProperty();
        bool in3dB = (mxB >= vX && mxB < vX + vW && myB >= vY && myB < vY + vH);

        if (leftBtn && !prevLeft && in3dB) {
            boxSelectX0_ = mxB; boxSelectY0_ = myB;
        }
        if (leftBtn && prevLeft && !boxSelectActive_ && in3dB) {
            int ddx = mxB - boxSelectX0_, ddy = myB - boxSelectY0_;
            if (std::abs(ddx) > 4 || std::abs(ddy) > 4)
                boxSelectActive_ = true;
        }
        if (boxSelectActive_ && leftBtn) {
            boxSelectX1_ = mxB; boxSelectY1_ = myB;
        }
        if (boxSelectActive_ && !leftBtn && prevLeft) {
            Matrix vwB = camera_.viewMatrix();
            Matrix prB = camera_.projectionMatrix(asp);
            Matrix vpB = vwB * prB;

            auto w2sB = [&](float wx, float wy, float wz) -> std::pair<float,float> {
                const auto yUp = coordinateToYUpAlg(document_.coordinateSystem, {wx, wy, wz});
                wx = yUp[0]; wy = yUp[1]; wz = yUp[2];
                float cX = wx*vpB.M11 + wy*vpB.M21 + wz*vpB.M31 + vpB.M41;
                float cY = wx*vpB.M12 + wy*vpB.M22 + wz*vpB.M32 + vpB.M42;
                float cW = wx*vpB.M14 + wy*vpB.M24 + wz*vpB.M34 + vpB.M44;
                if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
                return { (cX/cW * 0.5f + 0.5f) * vW + vX,
                         (1.0f - (cY/cW * 0.5f + 0.5f)) * vH + vY };
            };

            int bxMin = std::min(boxSelectX0_, mxB), bxMax = std::max(boxSelectX0_, mxB);
            int byMin = std::min(boxSelectY0_, myB), byMax = std::max(boxSelectY0_, myB);

            bool additive = (Keyboard::GetState().IsKeyDown(Keys::LeftControl) ||
                             Keyboard::GetState().IsKeyDown(Keys::RightControl));
            if (!additive) selection_.clear();

            std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> boxTest;
            boxTest = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
                for (const auto& obj : list) {
                    if (!obj || !obj->visible) continue;
                    auto [sx, sy] = w2sB(obj->transform.position[0],
                                         obj->transform.position[1],
                                         obj->transform.position[2]);
                    if (sx >= bxMin && sx <= bxMax && sy >= byMin && sy <= byMax)
                        selection_.select(obj);
                    if (!obj->children.empty()) boxTest(obj->children);
                }
            };
            boxTest(document_.objects);
            updateWindowTitle();
            boxSelectActive_ = false;
        }
    }
    if (activeTool_ != ActiveTool::Select) boxSelectActive_ = false;

    // Measurement tool: left-click places/advances points, right-click clears
    if (activeTool_ == ActiveTool::Measure) {
        int mx = ms.getXProperty(), my = ms.getYProperty();
        bool in3d = (mx >= vX && mx < vX + vW && my >= vY && my < vY + vH);

        if (rightBtn && !leftBtn) {
            mPt1Set_ = false; mPt2Set_ = false; mDist_ = 0.0f;
        }

        if (leftBtn && !prevLeft && in3d) {
            // Ray-cast to y=0 ground plane
            float ndcX = (static_cast<float>(mx - vX) / vW) * 2.0f - 1.0f;
            float ndcY = 1.0f - (static_cast<float>(my - vY) / vH) * 2.0f;
            Matrix vw = camera_.viewMatrix();
            Matrix pr = camera_.projectionMatrix(asp);
            Vector3 origin = camera_.position();
            Vector3 dir    = camera_.screenRayDirection(ndcX, ndcY, asp);
            (void)vw; (void)pr;

            std::array<float,3> hitPt{};
            bool gotHit = false;
            if (std::abs(dir.Y) > 1e-5f) {
                float t = -origin.Y / dir.Y;
                if (t > 0.0f) {
                    hitPt = { origin.X + t * dir.X, 0.0f, origin.Z + t * dir.Z };
                    gotHit = true;
                }
            }
            if (!gotHit) {
                // Fallback: use a plane at camera target height
                float planeY = camera_.target.Y;
                float dy = dir.Y;
                if (std::abs(dy) > 1e-5f) {
                    float t = (planeY - origin.Y) / dy;
                    if (t > 0.0f) {
                        hitPt = { origin.X + t*dir.X, planeY, origin.Z + t*dir.Z };
                        gotHit = true;
                    }
                }
            }

            if (gotHit) {
                if (!mPt1Set_) {
                    mPt1_ = hitPt;
                    mPt1Set_ = true;
                    mPt2Set_ = false;
                    mDist_ = 0.0f;
                } else {
                    mPt2_ = hitPt;
                    mPt2Set_ = true;
                    float dx2 = mPt2_[0]-mPt1_[0];
                    float dy2 = mPt2_[1]-mPt1_[1];
                    float dz2 = mPt2_[2]-mPt1_[2];
                    mDist_ = std::sqrt(dx2*dx2 + dy2*dy2 + dz2*dz2);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Scene / file operations
// ---------------------------------------------------------------------------


} // namespace MeshCraft::Application
