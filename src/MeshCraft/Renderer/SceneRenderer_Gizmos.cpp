#include "MeshCraft/Renderer/SceneRenderer.hpp"

#include <Microsoft/Xna/Framework/Graphics/BufferUsage.hpp>
#include <Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp>
#include <Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp>
#include <Microsoft/Xna/Framework/MathHelper.hpp>
#include <Microsoft/Xna/Framework/Vector2.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MeshCraft::Mc3;
using namespace MeshCraft::Renderer;

namespace MeshCraft::Renderer {

// ---------------------------------------------------------------------------
// Gizmo
// ---------------------------------------------------------------------------

void SceneRenderer::drawGizmo(const Mc3Object* obj,
                               const Matrix& view, const Matrix& proj,
                               float gizmoLength, bool localSpace)
{
    if (!obj) return;

    float px = obj->transform.position[0];
    float py = obj->transform.position[1];
    float pz = obj->transform.position[2];
    float L  = gizmoLength;

    // Compute axis directions (world or local)
    Vector3 axX{1,0,0}, axY{0,1,0}, axZ{0,0,1};
    if (localSpace) {
        const float d = std::numbers::pi_v<float> / 180.0f;
        Matrix rotM = Matrix::CreateFromYawPitchRoll(
            obj->transform.rotation[1]*d, obj->transform.rotation[0]*d, obj->transform.rotation[2]*d);
        axX = rotM.getRightProperty();
        axY = rotM.getUpProperty();
        // -Forward = +Z in local space
        Vector3 fwd = rotM.getForwardProperty();
        axZ = {-fwd.X, -fwd.Y, -fwd.Z};
    }

    float tx = px+L*axX.X, ty = py+L*axX.Y, tz = pz+L*axX.Z;
    float ux = px+L*axY.X, uy = py+L*axY.Y, uz = pz+L*axY.Z;
    float wx = px+L*axZ.X, wy = py+L*axZ.Y, wz = pz+L*axZ.Z;

    VertexPositionColor lineVerts[6] = {
        { {px, py, pz}, Color(210, 60,  60,  255) },
        { {tx, ty, tz}, Color(210, 60,  60,  255) },
        { {px, py, pz}, Color(60,  210, 60,  255) },
        { {ux, uy, uz}, Color(60,  210, 60,  255) },
        { {px, py, pz}, Color(60,  60,  210, 255) },
        { {wx, wy, wz}, Color(60,  60,  210, 255) },
    };

    VertexBuffer lineVB(device_, 6);
    lineVB.SetData(lineVerts, 6);

    effect_->World      = Matrix::getIdentityProperty();
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;

    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(&lineVB);
    device_.DrawPrimitives(Graphics::PrimitiveType::LineList, 0, 3);
    device_.SetVertexBuffer(nullptr);

    // Small cube at each axis tip as a drag handle
    float hs = L * 0.10f;
    Color tipCols[3] = {
        Color(210, 60,  60,  255),
        Color(60,  210, 60,  255),
        Color(60,  60,  210, 255),
    };
    float tips[3][3] = { {tx, ty, tz}, {ux, uy, uz}, {wx, wy, wz} };
    for (int i = 0; i < 3; ++i) {
        Matrix world = Matrix::CreateScale(hs) *
                       Matrix::CreateTranslation({tips[i][0], tips[i][1], tips[i][2]});
        drawMesh(unitBox_, world, view, proj, tipCols[i]);
    }
}

void SceneRenderer::drawScaleGizmo(const Mc3Object* obj,
                                    const Matrix& view, const Matrix& proj,
                                    float gizmoLength, bool localSpace)
{
    if (!obj) return;

    float px = obj->transform.position[0];
    float py = obj->transform.position[1];
    float pz = obj->transform.position[2];
    float L  = gizmoLength;

    Vector3 axX{1,0,0}, axY{0,1,0}, axZ{0,0,1};
    if (localSpace) {
        const float d = std::numbers::pi_v<float> / 180.0f;
        Matrix rotM = Matrix::CreateFromYawPitchRoll(
            obj->transform.rotation[1]*d, obj->transform.rotation[0]*d, obj->transform.rotation[2]*d);
        axX = rotM.getRightProperty();
        axY = rotM.getUpProperty();
        Vector3 fwd = rotM.getForwardProperty();
        axZ = {-fwd.X, -fwd.Y, -fwd.Z};
    }

    float tx = px+L*axX.X, ty = py+L*axX.Y, tz = pz+L*axX.Z;
    float ux = px+L*axY.X, uy = py+L*axY.Y, uz = pz+L*axY.Z;
    float wx = px+L*axZ.X, wy = py+L*axZ.Y, wz = pz+L*axZ.Z;

    VertexPositionColor lineVerts[6] = {
        { {px, py, pz}, Color(210, 60,  60,  255) },
        { {tx, ty, tz}, Color(210, 60,  60,  255) },
        { {px, py, pz}, Color(60,  210, 60,  255) },
        { {ux, uy, uz}, Color(60,  210, 60,  255) },
        { {px, py, pz}, Color(60,  60,  210, 255) },
        { {wx, wy, wz}, Color(60,  60,  210, 255) },
    };

    VertexBuffer lineVB(device_, 6);
    lineVB.SetData(lineVerts, 6);

    effect_->World      = Matrix::getIdentityProperty();
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;

    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(&lineVB);
    device_.DrawPrimitives(Graphics::PrimitiveType::LineList, 0, 3);
    device_.SetVertexBuffer(nullptr);

    // Flat-square tips
    float hs = L * 0.12f;
    float thin = hs * 0.25f;
    Color tipCols[3] = {
        Color(210, 60,  60,  255),
        Color(60,  210, 60,  255),
        Color(60,  60,  210, 255),
    };
    float tips[3][3] = { {tx, ty, tz}, {ux, uy, uz}, {wx, wy, wz} };
    Vector3 tipScales[3] = { {thin, hs, hs}, {hs, thin, hs}, {hs, hs, thin} };
    for (int i = 0; i < 3; ++i) {
        Matrix world = Matrix::CreateScale(tipScales[i]) *
                       Matrix::CreateTranslation({tips[i][0], tips[i][1], tips[i][2]});
        drawMesh(unitBox_, world, view, proj, tipCols[i]);
    }
}

void SceneRenderer::drawRotateGizmo(const Mc3Object* obj,
                                     const Matrix& view, const Matrix& proj,
                                     float gizmoLength, bool localSpace)
{
    if (!obj) return;

    const float px = obj->transform.position[0];
    const float py = obj->transform.position[1];
    const float pz = obj->transform.position[2];
    const float L  = gizmoLength;
    const int   N  = 32;

    // Local axis vectors (used as circle plane basis)
    Vector3 axX{1,0,0}, axY{0,1,0}, axZ{0,0,1};
    if (localSpace) {
        const float d = std::numbers::pi_v<float> / 180.0f;
        Matrix rotM = Matrix::CreateFromYawPitchRoll(
            obj->transform.rotation[1]*d, obj->transform.rotation[0]*d, obj->transform.rotation[2]*d);
        axX = rotM.getRightProperty();
        axY = rotM.getUpProperty();
        Vector3 fwd = rotM.getForwardProperty();
        axZ = {-fwd.X, -fwd.Y, -fwd.Z};
    }

    effect_->World      = Matrix::getIdentityProperty();
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;
    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    Color cols[3] = {
        Color(210, 60,  60,  255),
        Color(60,  210, 60,  255),
        Color(60,  60,  210, 255),
    };

    // For each axis: the circle lies in the plane spanned by the other two local axes
    Vector3 planeU[3] = { axY, axZ, axX };  // circle U-tangent for each ring
    Vector3 planeV[3] = { axZ, axX, axY };  // circle V-tangent for each ring

    for (int ax = 0; ax < 3; ++ax) {
        std::vector<VertexPositionColor> verts(N + 1);
        for (int j = 0; j <= N; ++j) {
            float t = 2.0f * std::numbers::pi_v<float> * j / N;
            float c = std::cos(t), s = std::sin(t);
            float wx = px + L*(c*planeU[ax].X + s*planeV[ax].X);
            float wy = py + L*(c*planeU[ax].Y + s*planeV[ax].Y);
            float wz = pz + L*(c*planeU[ax].Z + s*planeV[ax].Z);
            verts[j] = { {wx, wy, wz}, cols[ax] };
        }
        VertexBuffer circleVB(device_, N + 1);
        circleVB.SetData(verts.data(), N + 1);
        device_.SetVertexBuffer(&circleVB);
        device_.DrawPrimitives(Graphics::PrimitiveType::LineStrip, 0, N);
        device_.SetVertexBuffer(nullptr);
    }
}


} // namespace MeshCraft::Renderer
