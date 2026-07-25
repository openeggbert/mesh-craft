#include "MeshCraft/ImGuiRenderAlgorithms.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    int failures = 0;
    auto check = [&](bool value, const char* message) {
        if (value) std::cout << "PASS: " << message << "\n";
        else { std::cerr << "FAIL: " << message << "\n"; ++failures; }
    };

    const auto clipped = MeshCraft::makeImGuiScissorRect(-4.f, 3.2f, 22.1f, 50.f,
                                                           10.f, 2.f, 2.f, 2.f,
                                                           20.f, 30.f);
    check(clipped && clipped->x == 0 && clipped->y == 2 &&
          clipped->width == 20 && clipped->height == 28,
          "clip rectangle is display-relative, scaled, and bounded to the framebuffer");
    check(!MeshCraft::makeImGuiScissorRect(1.f, 1.f, 1.f, 3.f,
                                            0.f, 0.f, 1.f, 1.f, 20.f, 20.f),
          "empty clip rectangle is rejected");

    const std::uint16_t source[] = {9, 10, 11, 12, 13, 14};
    std::vector<std::uint16_t> localized;
    check(MeshCraft::localizeImGuiIndices(source, 6, 1, 3, 10, localized) &&
          localized == std::vector<std::uint16_t>{0, 1, 2},
          "indices are localized to ImDrawCmd::VtxOffset");
    check(!MeshCraft::localizeImGuiIndices(source, 6, 5, 2, 10, localized),
          "out-of-range index span is rejected");
    check(!MeshCraft::localizeImGuiIndices(source, 6, 0, 1, 10, localized),
          "index below VtxOffset is rejected rather than underflowing");
    return failures == 0 ? 0 : 1;
}
