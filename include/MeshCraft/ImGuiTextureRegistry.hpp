#pragma once

#include <cstdint>
#include <unordered_map>

namespace Microsoft::Xna::Framework::Graphics { class Texture2D; }

namespace MeshCraft {

// Backend-neutral ownership-free mapping between Dear ImGui's opaque texture
// token and the CNA texture that the renderer will bind for a draw command.
class ImGuiTextureRegistry {
public:
    std::uintptr_t registerTexture(Microsoft::Xna::Framework::Graphics::Texture2D* texture) {
        if (!texture) return 0;
        const std::uintptr_t token = nextToken_++;
        textures_.emplace(token, texture);
        return token;
    }

    Microsoft::Xna::Framework::Graphics::Texture2D* resolve(std::uintptr_t token) const {
        const auto found = textures_.find(token);
        return found == textures_.end() ? nullptr : found->second;
    }

    void unregisterTexture(std::uintptr_t token) { textures_.erase(token); }
    void clear() { textures_.clear(); }

private:
    std::unordered_map<std::uintptr_t, Microsoft::Xna::Framework::Graphics::Texture2D*> textures_;
    std::uintptr_t nextToken_{1};
};

} // namespace MeshCraft
