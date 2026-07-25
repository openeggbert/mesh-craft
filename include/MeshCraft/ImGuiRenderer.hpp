#pragma once

#include <cstdint>
#include <memory>

struct ImDrawData;
struct SDL_Window;

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
class Texture2D;
}

namespace MeshCraft {

// MeshCraft's rendering half of Dear ImGui. SDL remains the platform/input
// backend; this class deliberately renders ImDrawData only through CNA.
class ImGuiRenderer {
public:
    virtual ~ImGuiRenderer() = default;
    virtual bool initialize(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                            SDL_Window* window) = 0;
    virtual void newFrame() = 0;
    virtual void render(ImDrawData* drawData) = 0;
    virtual void shutdown() = 0;

    // ImTextureID is opaque. MeshCraft stores this integer token rather than
    // leaking a native API handle into panels that call ImGui::Image().
    virtual std::uintptr_t registerTexture(Microsoft::Xna::Framework::Graphics::Texture2D& texture) = 0;
    virtual void unregisterTexture(std::uintptr_t token) = 0;

    static std::unique_ptr<ImGuiRenderer> createCnaRenderer();
};

} // namespace MeshCraft
