#pragma once

#include <optional>
#include <string>
#include <vector>

namespace MeshCraft::Mc3 {

enum class Interpolation { Step, Linear, CubicBezier };

// Which property of an object a channel animates.
// All properties are float-valued; Visible uses 0.0 = false, >= 0.5 = true.
enum class AnimatedProperty {
    PositionX, PositionY, PositionZ,
    RotationX, RotationY, RotationZ,
    ScaleX,    ScaleY,    ScaleZ,
    Visible,
    DeformX,   DeformY,   DeformZ,
    MaterialBaseColorR, MaterialBaseColorG, MaterialBaseColorB, MaterialBaseColorA,
    MaterialRoughness, MaterialMetallic,
    MaterialEmissiveR, MaterialEmissiveG, MaterialEmissiveB,
};

// Bezier tangent handle — offset relative to the owning keyframe (time, value).
struct Mc3BezierHandle {
    float dt{ 0.0f };
    float dv{ 0.0f };
};

// A single keyframe in a channel.
// handleLeft / handleRight are used only for CubicBezier interpolation.
struct Mc3Keyframe {
    float          time{ 0.0f };
    float          value{ 0.0f };
    Interpolation  interpolation{ Interpolation::Linear };
    Mc3BezierHandle handleLeft { -0.1f, 0.0f };
    Mc3BezierHandle handleRight{  0.1f, 0.0f };
};

// A channel animates one property of one named scene object.
// Keyframes must be kept sorted by time (ascending).
struct Mc3Channel {
    std::string      targetObject;
    AnimatedProperty property{ AnimatedProperty::PositionX };
    std::vector<Mc3Keyframe> keyframes;
};

// A named animation clip containing any number of channels.
struct Mc3Action {
    std::string name;
    float       duration{ 1.0f };
    bool        loop{ false };
    std::vector<Mc3Channel> channels;
};

// Property name ↔ enum conversions (XML attribute values).
const char*                     animatedPropertyName(AnimatedProperty p);
std::optional<AnimatedProperty> animatedPropertyFromName(const std::string& name);

// Evaluate a channel at the given time.  Returns 0.0f if there are no keyframes.
float evaluateChannel(const Mc3Channel& ch, float time);

} // namespace MeshCraft::Mc3
