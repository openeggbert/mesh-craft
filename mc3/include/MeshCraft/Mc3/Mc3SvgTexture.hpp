#pragma once

#include <string>

namespace MeshCraft::Mc3 {

// SVG texture — either references an external .svg file or embeds SVG markup inline.
// XML:  <texture type="svg" src="logo.svg"/>
//       <texture type="svg"><![CDATA[<svg>…</svg>]]></texture>
struct Mc3SvgTexture {
    std::string id;
    std::string src;            // path to external .svg (empty when inline)
    std::string inlineContent;  // raw SVG markup (empty when external)

    [[nodiscard]] bool isInline()   const { return src.empty() && !inlineContent.empty(); }
    [[nodiscard]] bool isExternal() const { return !src.empty(); }
};

} // namespace MeshCraft::Mc3
