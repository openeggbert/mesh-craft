#pragma once

#include <Microsoft/Xna/Framework/Color.hpp>
#include <functional>
#include <string>

namespace MeshCraft::Ui {

using Microsoft::Xna::Framework::Color;
using FillFn = std::function<void(int x, int y, int w, int h, Color)>;

// Draw ASCII text with an embedded 5×7 pixel bitmap font.
// Each glyph cell is (5*scale + 1) px wide, 7*scale px tall.
// fillRect is invoked for each lit pixel block.
void drawBitmapText(const std::string& text, int x, int y, int scale, Color color,
                    const FillFn& fillRect);

// Pixel width of text at the given scale (without trailing gap).
int bitmapTextWidth(const std::string& text, int scale);

} // namespace MeshCraft::Ui
