#include "MeshCraft/Renderer/GridRenderer.hpp"

#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Graphics/BlendState.hpp>
#include <Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <algorithm>
#include <cmath>
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

void GridRenderer::setSpacing(float spacing) {
    int halfExtent = std::max(4, static_cast<int>(20.0f / spacing));
    buildGrid(halfExtent, spacing);
}

void GridRenderer::buildGrid(int halfExtent, float spacing) {
    std::vector<VertexPositionColor> verts;
    verts.reserve(4 * (2 * halfExtent + 1));

    float ext = halfExtent * spacing;

    for (int i = -halfExtent; i <= halfExtent; ++i) {
        float pos = i * spacing;
        // Alpha fades quadratically from center (full) to edge (transparent)
        float distFactor = static_cast<float>(std::abs(i)) / static_cast<float>(std::max(halfExtent, 1));
        int fadeAlpha = static_cast<int>(
            std::clamp(1.0f - distFactor * distFactor, 0.0f, 1.0f) * 255.0f);

        Color colX = (i == 0) ? kAxisZ
                   : ((i % 5 == 0) ? Color(120, 120, 120, fadeAlpha)
                                   : Color(80,  80,  80,  fadeAlpha));
        Color colZ = (i == 0) ? kAxisX
                   : ((i % 5 == 0) ? Color(120, 120, 120, fadeAlpha)
                                   : Color(80,  80,  80,  fadeAlpha));

        // Line along Z axis at x=pos
        verts.push_back({ Vector3{pos, 0.0f, -ext}, colX });
        verts.push_back({ Vector3{pos, 0.0f,  ext}, colX });

        // Line along X axis at z=pos
        verts.push_back({ Vector3{-ext, 0.0f, pos}, colZ });
        verts.push_back({ Vector3{ ext, 0.0f, pos}, colZ });
    }

    // Y axis
    verts.push_back({ Vector3{0.0f, 0.0f,   0.0f}, kAxisY });
    verts.push_back({ Vector3{0.0f, ext,    0.0f}, kAxisY });

    lineCount_ = static_cast<int>(verts.size()) / 2;
    vb_ = std::make_unique<VertexBuffer>(device_, static_cast<int>(verts.size()));
    vb_->SetData(verts.data(), static_cast<int>(verts.size()));
}

void GridRenderer::draw(const Matrix& view, const Matrix& projection) {
    device_.SetBlendEnabled(true);
    device_.setBlendStateProperty(Microsoft::Xna::Framework::Graphics::BlendState::AlphaBlend);

    effect_->World      = Matrix::getIdentityProperty();
    effect_->View       = view;
    effect_->Projection = projection;

    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty()) {
        pass.Apply();
    }

    device_.SetVertexBuffer(vb_.get());
    device_.DrawPrimitives(Microsoft::Xna::Framework::Graphics::PrimitiveType::LineList, 0, lineCount_);
    device_.SetVertexBuffer(nullptr);

    device_.SetBlendEnabled(false);
}

} // namespace MeshCraft::Renderer
