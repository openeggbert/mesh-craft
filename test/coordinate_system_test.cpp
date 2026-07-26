// SYS-W14-14: CNA-free regression coverage for coordinate conversion,
// picking-ray inversion and the explicit Normalize to Y-up command.

#include "MeshCraft/CoordinateSystemAlgorithms.hpp"
#include "MeshCraft/EditorAlgorithms.hpp"

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

    if (failures == 0) std::puts("All coordinate-system tests passed.");
    else std::fprintf(stderr, "%d coordinate-system test(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
