#include "MeshCraft/ImGuiRenderer.hpp"
#include "MeshCraft/ImGuiRenderAlgorithms.hpp"
#include "MeshCraft/ImGuiTextureRegistry.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Rectangle.hpp>
#include <Microsoft/Xna/Framework/Vector2.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <Microsoft/Xna/Framework/Graphics/BasicEffect.hpp>
#include <Microsoft/Xna/Framework/Graphics/BlendState.hpp>
#include <Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
#include <Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp>
#include <Microsoft/Xna/Framework/Graphics/RasterizerState.hpp>
#include <Microsoft/Xna/Framework/Graphics/SamplerState.hpp>
#include <Microsoft/Xna/Framework/Graphics/Texture2D.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp>

#include <optional>
#include <vector>

namespace MeshCraft {
using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {
class CnaImGuiRenderer final : public ImGuiRenderer {
public:
    bool initialize(GraphicsDevice& device, SDL_Window* window) override {
        device_ = &device;
        if (!ImGui_ImplSDL3_InitForOther(window)) return false;
        effect_ = std::make_unique<BasicEffect>(device);
        effect_->setLightingEnabledProperty(false);
        effect_->setTextureEnabledProperty(true);
        effect_->VertexColorEnabled = true;

        unsigned char* pixels = nullptr;
        int w = 0, h = 0;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
        std::vector<std::uint8_t> rgba(pixels, pixels + static_cast<std::size_t>(w) * h * 4);
        fontTexture_.emplace(Texture2D::CreateFromPixels(device, w, h, rgba));
        ImGui::GetIO().Fonts->SetTexID(static_cast<ImTextureID>(registerTexture(*fontTexture_)));
        return true;
    }

    void newFrame() override { ImGui_ImplSDL3_NewFrame(); }

    void render(ImDrawData* drawData) override {
        if (!device_ || !effect_ || !drawData || drawData->TotalVtxCount <= 0) return;
        const float fbW = drawData->DisplaySize.x * drawData->FramebufferScale.x;
        const float fbH = drawData->DisplaySize.y * drawData->FramebufferScale.y;
        if (fbW <= 0.f || fbH <= 0.f) return;

        const BlendState oldBlend = device_->getBlendStateProperty();
        const DepthStencilState oldDepth = device_->getDepthStencilStateProperty();
        const RasterizerState oldRasterizer = device_->getRasterizerStateProperty();
        const Rectangle oldScissor = device_->getScissorRectangleProperty();
        const SamplerState oldSampler = device_->getSamplerStatesProperty()[0];

        BlendState blend;
        blend.setColorSourceBlendProperty(Blend::SourceAlpha);
        blend.setColorDestinationBlendProperty(Blend::InverseSourceAlpha);
        blend.setAlphaSourceBlendProperty(Blend::One);
        blend.setAlphaDestinationBlendProperty(Blend::InverseSourceAlpha);
        device_->setBlendStateProperty(blend);
        device_->SetDepthTestEnabled(false);
        device_->SetDepthWriteEnabled(false);
        RasterizerState rasterizer = oldRasterizer;
        rasterizer.setScissorTestEnableProperty(true);
        device_->setRasterizerStateProperty(rasterizer);
        device_->getSamplerStatesProperty()[0] = SamplerState::LinearClamp;

        effect_->World = Matrix::getIdentityProperty();
        effect_->View = Matrix::getIdentityProperty();
        effect_->Projection = Matrix::CreateOrthographicOffCenter(
            drawData->DisplayPos.x, drawData->DisplayPos.x + drawData->DisplaySize.x,
            drawData->DisplayPos.y + drawData->DisplaySize.y, drawData->DisplayPos.y,
            0.f, 1.f);

        for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex) {
            const ImDrawList* list = drawData->CmdLists[listIndex];
            if (!list) continue;
            std::vector<VertexPositionColorTexture> vertices;
            vertices.reserve(list->VtxBuffer.Size);
            for (const ImDrawVert& v : list->VtxBuffer) {
                const auto* c = reinterpret_cast<const std::uint8_t*>(&v.col);
                vertices.emplace_back(Vector3(v.pos.x, v.pos.y, 0.f), Color(c[0], c[1], c[2], c[3]),
                                      Vector2(v.uv.x, v.uv.y));
            }
            for (const ImDrawCmd& cmd : list->CmdBuffer) {
                if (cmd.UserCallback) {
                    if (cmd.UserCallback == ImDrawCallback_ResetRenderState) continue;
                    continue; // Native callbacks are intentionally unsupported by this portable renderer.
                }
                const ImVec4 clip = cmd.ClipRect;
                const auto scissor = makeImGuiScissorRect(
                    clip.x, clip.y, clip.z, clip.w, drawData->DisplayPos.x,
                    drawData->DisplayPos.y, drawData->FramebufferScale.x,
                    drawData->FramebufferScale.y, fbW, fbH);
                if (!scissor || cmd.ElemCount == 0 || cmd.ElemCount % 3 != 0 ||
                    cmd.VtxOffset > static_cast<unsigned int>(vertices.size()))
                    continue;
                Texture2D* texture = textures_.resolve(static_cast<std::uintptr_t>(cmd.GetTexID()));
                if (!texture) continue;
                // CNA's public DrawUserIndexedPrimitives has no base-vertex
                // argument. Localise this command's indices explicitly, the
                // same operation the native ImGui renderers express with
                // VtxOffset/base-vertex.
                std::vector<ImDrawIdx> indices;
                if (!localizeImGuiIndices(list->IdxBuffer.Data,
                                          static_cast<unsigned int>(list->IdxBuffer.Size),
                                          cmd.IdxOffset, cmd.ElemCount, cmd.VtxOffset, indices))
                    continue;
                effect_->setTextureProperty(texture);
                effect_->Apply();
                device_->setScissorRectangleProperty(
                    Rectangle(scissor->x, scissor->y, scissor->width, scissor->height));
                device_->DrawUserIndexedPrimitives(PrimitiveType::TriangleList, vertices.data(),
                    static_cast<int>(cmd.VtxOffset), static_cast<int>(vertices.size() - cmd.VtxOffset),
                    indices.data(), 0,
                    static_cast<int>(cmd.ElemCount / 3));
            }
        }
        effect_->setTextureProperty(nullptr);
        device_->getSamplerStatesProperty()[0] = oldSampler;
        device_->setScissorRectangleProperty(oldScissor);
        device_->setRasterizerStateProperty(oldRasterizer);
        device_->setDepthStencilStateProperty(oldDepth);
        device_->setBlendStateProperty(oldBlend);
    }

    void shutdown() override {
        textures_.clear();
        fontTexture_.reset();
        effect_.reset();
        device_ = nullptr;
        ImGui_ImplSDL3_Shutdown();
    }

    std::uintptr_t registerTexture(Texture2D& texture) override {
        return textures_.registerTexture(&texture);
    }
    void unregisterTexture(std::uintptr_t token) override { textures_.unregisterTexture(token); }

private:
    GraphicsDevice* device_{};
    std::unique_ptr<BasicEffect> effect_;
    std::optional<Texture2D> fontTexture_;
    ImGuiTextureRegistry textures_;
};
} // namespace

std::unique_ptr<ImGuiRenderer> ImGuiRenderer::createCnaRenderer() {
    return std::make_unique<CnaImGuiRenderer>();
}
} // namespace MeshCraft
