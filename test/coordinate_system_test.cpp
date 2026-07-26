// SYS-W14-14: CNA-free regression coverage for coordinate conversion,
// picking-ray inversion and the explicit Normalize to Y-up command.

#include "MeshCraft/CoordinateSystemAlgorithms.hpp"
#include "MeshCraft/EditorCommandAlgorithms.hpp"
#include "MeshCraft/EditorEventAlgorithms.hpp"
#include "MeshCraft/EditorSelectionAlgorithms.hpp"

#include <cmath>
#include <cstdio>
#include <memory>

using namespace MeshCraft;
using namespace MeshCraft::Mc3;

namespace {

bool close(float actual, float expected) {
    return std::abs(actual - expected) < 1.0e-5f;
}

bool same(const std::array<float, 3>& actual, const std::array<float, 3>& expected) {
    return close(actual[0], expected[0]) && close(actual[1], expected[1]) &&
           close(actual[2], expected[2]);
}

void expect(bool value, const char* message, int& failures) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

} // namespace

int main() {
    int failures = 0;

    // Transform fixture: Z-up's +Z maps to native +Y while handedness is
    // preserved by mapping authored +Y to native -Z.
    const auto yUp = coordinateToYUpAlg(kRightHandedZUpCoordinateSystem, {2.0f, 3.0f, 5.0f});
    expect(same(yUp, {2.0f, 5.0f, -3.0f}), "Z-up point converts to Y-up", failures);
    expect(same(coordinateFromYUpAlg(kRightHandedZUpCoordinateSystem, yUp), {2.0f, 3.0f, 5.0f}),
           "coordinate conversion has an exact inverse", failures);
    expect(same(coordinateToYUpAlg(kRightHandedYUpCoordinateSystem, {2.0f, 3.0f, 5.0f}),
                {2.0f, 3.0f, 5.0f}),
           "Y-up documents are unchanged", failures);

    // Picking fixture: a viewport ray in native Y-up is inverted before the
    // existing authored-space AABB picker runs. The object is at +Z in the
    // source document, therefore it appears above the origin in Y-up.
    auto pickBox = std::make_shared<Mc3Object>();
    pickBox->name = "pick-target";
    pickBox->transform.position = {0.0f, 0.0f, 5.0f};
    const auto authoredOrigin = coordinateFromYUpAlg(kRightHandedZUpCoordinateSystem,
                                                     {0.0f, 10.0f, 0.0f});
    const auto authoredDirection = coordinateFromYUpAlg(kRightHandedZUpCoordinateSystem,
                                                        {0.0f, -1.0f, 0.0f});
    expect(pickObjectByRayAlg({pickBox}, authoredOrigin, authoredDirection) == pickBox,
           "Y-up viewport ray selects the Z-up authored object", failures);

    Mc3Document document;
    document.coordinateSystem = std::string(kRightHandedZUpCoordinateSystem);
    document.objects.push_back(pickBox);
    document.lights.push_back(Mc3Light::spot("spot", {1.0f, 2.0f, 3.0f}, {0.0f, -1.0f, 0.0f}));
    Mc3Camera camera;
    camera.position = {4.0f, 5.0f, 6.0f};
    camera.rotation = std::array<float, 3>{0.0f, 0.0f, 0.0f};
    document.cameras.push_back(camera);

    expect(normalizeCoordinateSystemToYUpAlg(document), "Z-up document normalizes", failures);
    expect(document.coordinateSystem == kRightHandedYUpCoordinateSystem,
           "normalization changes declaration to Y-up", failures);
    expect(document.objects.size() == 1 && document.objects.front()->type == ObjectType::Group &&
               close(document.objects.front()->transform.rotation[0], -90.0f) &&
               document.objects.front()->children.size() == 1 &&
               document.objects.front()->children.front() == pickBox,
           "normalization preserves objects under an explicit conversion group", failures);
    expect(same(document.lights.front().position, {1.0f, 3.0f, -2.0f}) &&
               same(document.lights.front().direction, {0.0f, 0.0f, 1.0f}),
           "normalization converts light position and direction", failures);
    expect(same(document.cameras.front().position, {4.0f, 6.0f, -5.0f}) &&
               !document.cameras.front().rotation.has_value(),
           "normalization converts cameras and removes lossy Euler rotation", failures);
    expect(!normalizeCoordinateSystemToYUpAlg(document), "Y-up normalization is idempotent", failures);

    // Rotation normalization is deliberately lossless for static rotations:
    // transform, object-state, definition and camera values are all baked to
    // degrees/XYZ while their matrices remain identical.
    Mc3Document rotations;
    rotations.rotationUnits = "radians";
    rotations.eulerOrder = "YZX";
    auto rotatedObject = std::make_shared<Mc3Object>();
    rotatedObject->transform.rotation = {0.25f, -0.5f, 0.75f};
    rotatedObject->states["open"].rotation = std::array<float, 3>{-0.3f, 0.2f, 0.1f};
    auto definition = std::make_shared<Mc3Object>();
    definition->transform.rotation = {0.4f, 0.1f, -0.2f};
    rotations.objects.push_back(rotatedObject);
    rotations.definitions["definition"] = definition;
    Mc3Camera rotationCamera;
    rotationCamera.rotation = std::array<float, 3>{0.1f, 0.2f, 0.3f};
    rotations.cameras.push_back(rotationCamera);
    const auto objectMatrix = rotationMatrix3Alg(rotatedObject->transform.rotation,
                                                 rotations.rotationUnits, rotations.eulerOrder);
    const auto stateMatrix = rotationMatrix3Alg(*rotatedObject->states["open"].rotation,
                                                rotations.rotationUnits, rotations.eulerOrder);
    const auto definitionMatrix = rotationMatrix3Alg(definition->transform.rotation,
                                                     rotations.rotationUnits, rotations.eulerOrder);
    const auto cameraMatrix = rotationMatrix3Alg(*rotationCamera.rotation,
                                                 rotations.rotationUnits, rotations.eulerOrder);
    expect(normalizeRotationConventionToDegreesXYZAlg(rotations),
           "static rotation convention normalizes", failures);
    expect(rotations.rotationUnits == "degrees" && rotations.eulerOrder == "XYZ",
           "rotation normalization declares degrees/XYZ", failures);
    const auto matrixSame = [](const RotationMatrix3Alg& left, const RotationMatrix3Alg& right) {
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                if (!close(left[row][col], right[row][col])) return false;
        return true;
    };
    expect(matrixSame(objectMatrix, rotationMatrix3Alg(rotatedObject->transform.rotation,
                                                       rotations.rotationUnits, rotations.eulerOrder)) &&
               matrixSame(stateMatrix, rotationMatrix3Alg(*rotatedObject->states["open"].rotation,
                                                          rotations.rotationUnits, rotations.eulerOrder)) &&
               matrixSame(definitionMatrix, rotationMatrix3Alg(definition->transform.rotation,
                                                               rotations.rotationUnits, rotations.eulerOrder)) &&
               matrixSame(cameraMatrix, rotationMatrix3Alg(*rotations.cameras.front().rotation,
                                                           rotations.rotationUnits, rotations.eulerOrder)),
           "normalization preserves all static rotation matrices", failures);

    Mc3Document animatedRotations;
    animatedRotations.rotationUnits = "radians";
    animatedRotations.eulerOrder = "ZYX";
    Mc3Action animatedAction;
    animatedAction.channels.push_back({"target", AnimatedProperty::RotationY,
                                       {Mc3Keyframe::linear(0.0f, 0.0f),
                                        Mc3Keyframe::linear(1.0f, 1.0f)}});
    animatedRotations.actions["turn"] = animatedAction;
    expect(hasAnimatedRotationAlg(animatedRotations) &&
               !normalizeRotationConventionToDegreesXYZAlg(animatedRotations) &&
               animatedRotations.rotationUnits == "radians" && animatedRotations.eulerOrder == "ZYX",
           "normalization refuses animated Euler rotations rather than changing motion", failures);

    // Picking uses the same radian/Euler transform through both a parent and
    // a pivot. The child lands at x=2,z=1 only after the complete hierarchy
    // is composed, so this also catches a regression to local-only picking.
    auto rotatedParent = std::make_shared<Mc3Object>();
    rotatedParent->type = ObjectType::Group;
    rotatedParent->transform.position = {2.0f, 0.0f, 0.0f};
    rotatedParent->transform.rotation = {0.0f, std::numbers::pi_v<float> * 0.5f, 0.0f};
    rotatedParent->transform.pivot = {1.0f, 0.0f, 0.0f};
    auto rotatedChild = std::make_shared<Mc3Object>();
    rotatedChild->transform.position = {0.0f, 0.0f, -1.0f};
    rotatedParent->children.push_back(rotatedChild);
    expect(pickObjectByRayAlg({rotatedParent}, {2.0f, 0.0f, -10.0f}, {0.0f, 0.0f, 1.0f},
                              "radians", "XYZ") == rotatedChild,
           "radian picking composes nested transforms and pivots", failures);

    // Animation writes the authored raw values into an override; the
    // renderer then interprets that triple with the same document convention
    // as static transforms. This is a headless differential check of that
    // hand-off for a non-XYZ radian document.
    Mc3Document animationDocument;
    animationDocument.rotationUnits = "radians";
    animationDocument.eulerOrder = "ZXY";
    auto animatedObject = std::make_shared<Mc3Object>();
    animatedObject->name = "animated";
    animatedObject->transform.rotation = {0.2f, 0.3f, 0.4f};
    Mc3Action animationAction;
    animationAction.channels.push_back({"animated", AnimatedProperty::RotationY,
                                        {Mc3Keyframe::step(0.0f, -0.6f)}});
    const auto overrides = computeAnimOverridesAlg(
        animationAction, 0.0f, animationDocument.materials,
        [&](const std::string& name) -> Mc3Object* {
            return name == animatedObject->name ? animatedObject.get() : nullptr;
        });
    const auto animationOverride = overrides.find("animated");
    expect(animationOverride != overrides.end() && animationOverride->second.rotation.has_value() &&
               matrixSame(rotationMatrix3Alg(*animationOverride->second.rotation,
                                             animationDocument.rotationUnits,
                                             animationDocument.eulerOrder),
                          rotationMatrix3Alg({0.2f, -0.6f, 0.4f}, "radians", "ZXY")),
           "animated rotation override retains the document convention", failures);

    if (failures == 0) std::puts("All coordinate-system tests passed.");
    else std::fprintf(stderr, "%d coordinate-system test(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
