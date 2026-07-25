#include "MeshCraft/ImGuiTextureRegistry.hpp"

#include <iostream>

using MeshCraft::ImGuiTextureRegistry;
using Microsoft::Xna::Framework::Graphics::Texture2D;

int main() {
    int failures = 0;
    auto check = [&](bool value, const char* message) {
        if (value) std::cout << "PASS: " << message << "\n";
        else { std::cerr << "FAIL: " << message << "\n"; ++failures; }
    };

    ImGuiTextureRegistry registry;
    int firstStorage = 0;
    int secondStorage = 0;
    auto* first = reinterpret_cast<Texture2D*>(&firstStorage);
    auto* second = reinterpret_cast<Texture2D*>(&secondStorage);
    const auto firstToken = registry.registerTexture(first);
    const auto secondToken = registry.registerTexture(second);
    const auto duplicateToken = registry.registerTexture(first);
    check(firstToken != 0 && firstToken != secondToken, "tokens are non-zero and unique");
    check(duplicateToken != firstToken && registry.resolve(duplicateToken) == first,
          "duplicate registration receives a distinct opaque token");
    check(registry.resolve(firstToken) == first, "first token resolves its CNA texture");
    check(registry.resolve(secondToken) == second, "second token resolves its CNA texture");
    registry.unregisterTexture(firstToken);
    check(registry.resolve(firstToken) == nullptr, "unregistered token becomes stale");
    check(registry.resolve(duplicateToken) == first, "one stale token does not invalidate a duplicate registration");
    check(registry.resolve(secondToken) == second, "unregistering one token preserves another");
    registry.clear();
    check(registry.resolve(secondToken) == nullptr, "clear invalidates all tokens");
    return failures == 0 ? 0 : 1;
}
