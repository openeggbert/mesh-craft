#include "MeshCraft/Renderer/GridRenderer.hpp"

#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <vector>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace MeshCraft::Renderer {

static Color kGridMinor(80, 80, 80, 255);
static Color kGridMajor(120, 120, 120, 255);
static Color kAxisX(200, 60, 60, 255);
static Color kAxisZ(60, 60, 200, 255);
static Color kAxisY(60, 200, 60, 255);

GridRenderer::GridRenderer(GraphicsDevice& device)
    : device_(device)
{
    buildGrid(20, 1.0f);

    effect_ = std::make_unique<BasicEffect>(device_);
    effect_->VertexColorEnabled = true;
}

void GridRenderer::buildGrid(int halfExtent, float spacing) {
    std::vector<VertexPositionColor> verts;
    verts.reserve(4 * (2 * halfExtent + 1));

    float ext = halfExtent * spacing;

    for (int i = -halfExtent; i <= halfExtent; ++i) {
        float t = i * spacing;
        Color colX = (i == 0) ? kAxisZ : ((i % 5 == 0) ? kGridMajor : kGridMinor);
        Color colZ = (i == 0) ? kAxisX : ((i % 5 == 0) ? kGridMajor : kGridMinor);

        // Line along Z axis at x=t
        verts.push_back({ Vector3{t, 0.0f, -ext}, colX });
        verts.push_back({ Vector3{t, 0.0f,  ext}, colX });

        // Line along X axis at z=t
        verts.push_back({ Vector3{-ext, 0.0f, t}, colZ });
        verts.push_back({ Vector3{ ext, 0.0f, t}, colZ });
    }

    // Y axis
    verts.push_back({ Vector3{0.0f, 0.0f,   0.0f}, kAxisY });
    verts.push_back({ Vector3{0.0f, ext,    0.0f}, kAxisY });

    lineCount_ = static_cast<int>(verts.size()) / 2;
    vb_ = std::make_unique<VertexBuffer>(device_, static_cast<int>(verts.size()));
    vb_->SetData(verts.data(), static_cast<int>(verts.size()));
}

void GridRenderer::draw(const Matrix& view, const Matrix& projection) {
    effect_->World      = Matrix::getIdentityProperty();
    effect_->View       = view;
    effect_->Projection = projection;

    for (auto& pass : effect_->CurrentTechnique().Passes()) {
        pass.Apply();
    }

    device_.SetVertexBuffer(vb_.get());
    device_.DrawPrimitives(Microsoft::Xna::Framework::Graphics::PrimitiveType::LineList, 0, lineCount_);
    device_.SetVertexBuffer(nullptr);
}

} // namespace MeshCraft::Renderer
