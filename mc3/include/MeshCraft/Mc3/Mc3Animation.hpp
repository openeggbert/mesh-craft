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

    // --- Factory helpers --------------------------------------------------
    static Mc3Keyframe linear(float t, float v) {
        Mc3Keyframe k; k.time = t; k.value = v;
        k.interpolation = Interpolation::Linear; return k;
    }
    static Mc3Keyframe step(float t, float v) {
        Mc3Keyframe k; k.time = t; k.value = v;
        k.interpolation = Interpolation::Step; return k;
    }
    static Mc3Keyframe bezier(float t, float v,
                              Mc3BezierHandle left  = {-0.1f, 0.f},
                              Mc3BezierHandle right = { 0.1f, 0.f}) {
        Mc3Keyframe k; k.time = t; k.value = v;
        k.interpolation = Interpolation::CubicBezier;
        k.handleLeft = left; k.handleRight = right; return k;
    }
};

// A channel animates one property of one named scene object.
// Keyframes must be kept sorted by time (ascending): evaluateChannel() below
// does a std::upper_bound binary search over this vector and will silently
// return wrong interpolation results if it is not sorted. This is currently
// maintained by Mc3XmlParser.cpp's parseActions() (stable_sort at parse
// time) and by the editor's keyframe-drag UI (MeshCraftApplication_Anim.cpp,
// stable_sort on mouse-release). Any other code that mutates `keyframes`
// directly must preserve this invariant, e.g. by re-sorting afterward.
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
    bool        autoplay{ false }; // start automatically when the scene is opened
    // STAB-0460: playback-speed multiplier (2.0 = twice as fast, 0.5 = half
    // speed). Multiplies dt when the editor advances its playback clock
    // (MeshCraftApplication.cpp); channel/keyframe times themselves stay in
    // the action's own unscaled time domain. Defaults to 1.0 (no-op),
    // matching every scene authored before this field existed.
    float       timeScale{ 1.0f };
    std::vector<Mc3Channel> channels;

    // --- Builder helpers --------------------------------------------------
    static Mc3Action make(std::string name, float duration = 1.0f,
                          bool loop = false, bool autoplay = false) {
        Mc3Action a; a.name = std::move(name);
        a.duration = duration; a.loop = loop; a.autoplay = autoplay; return a;
    }
    Mc3Action& addChannel(std::string target, AnimatedProperty prop,
                          std::vector<Mc3Keyframe> kfs) {
        Mc3Channel ch; ch.targetObject = std::move(target);
        ch.property = prop; ch.keyframes = std::move(kfs);
        channels.push_back(std::move(ch)); return *this;
    }
    Mc3Action& withLoop(bool v = true)     { loop     = v; return *this; }
    Mc3Action& withAutoplay(bool v = true) { autoplay = v; return *this; }
    Mc3Action& withTimeScale(float v)      { timeScale = v; return *this; }
};

// Property name ↔ enum conversions (XML attribute values).
const char*                     animatedPropertyName(AnimatedProperty p);
std::optional<AnimatedProperty> animatedPropertyFromName(const std::string& name);

// Evaluate a channel at the given time.  Returns 0.0f if there are no keyframes.
// Precondition: ch.keyframes must be sorted by ascending time (see the
// Mc3Channel::keyframes comment) -- this function binary-searches it.
float evaluateChannel(const Mc3Channel& ch, float time);

} // namespace MeshCraft::Mc3
