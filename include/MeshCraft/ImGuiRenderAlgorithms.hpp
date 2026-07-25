#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace MeshCraft {

struct ImGuiScissorRect {
    int x;
    int y;
    int width;
    int height;
};

// Converts Dear ImGui's display-relative clip rectangle to a bounded CNA
// scissor rectangle. Kept graphics-context-free for regression tests.
inline std::optional<ImGuiScissorRect> makeImGuiScissorRect(
    float left, float top, float right, float bottom,
    float displayX, float displayY, float scaleX, float scaleY,
    float framebufferWidth, float framebufferHeight)
{
    const int x = std::max(0, static_cast<int>(std::floor((left - displayX) * scaleX)));
    const int y = std::max(0, static_cast<int>(std::floor((top - displayY) * scaleY)));
    const int r = std::min(static_cast<int>(framebufferWidth),
                           static_cast<int>(std::ceil((right - displayX) * scaleX)));
    const int b = std::min(static_cast<int>(framebufferHeight),
                           static_cast<int>(std::ceil((bottom - displayY) * scaleY)));
    if (r <= x || b <= y) return std::nullopt;
    return ImGuiScissorRect{x, y, r - x, b - y};
}

// CNA user-indexed draws have no base-vertex parameter. Copy the command's
// indices into a short-lived local range relative to its vertex offset.
template <typename Index>
inline bool localizeImGuiIndices(const Index* source, unsigned int sourceCount,
                                 unsigned int indexOffset, unsigned int elementCount,
                                 unsigned int vertexOffset, std::vector<Index>& destination)
{
    if (indexOffset > sourceCount || elementCount > sourceCount - indexOffset)
        return false;
    destination.resize(elementCount);
    for (unsigned int i = 0; i < elementCount; ++i) {
        const Index index = source[indexOffset + i];
        if (index < vertexOffset) return false;
        destination[i] = static_cast<Index>(index - vertexOffset);
    }
    return true;
}

} // namespace MeshCraft
